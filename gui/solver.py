"""Perspective pose and focal fitting in original photo pixels; no lens calibration."""
import numpy as np
import cv2
from scipy.linalg import rq
from scipy.optimize import least_squares
from scipy.spatial.transform import Rotation


def fit_camera(world, pixels, width, height, initial_camera=None, estimate_principal=False):
    world, pixels = np.asarray(world, dtype=float), np.asarray(pixels, dtype=float)
    if world.ndim != 2 or world.shape[1:] != (3,) or len(world) < 6 or pixels.shape != (len(world), 2):
        raise ValueError("모델 점 6개 이상을 연결하세요.")
    if not np.isfinite(world).all() or not np.isfinite(pixels).all() or min(width, height) <= 0:
        raise ValueError("올바른 대응점을 선택하세요.")
    if len(np.unique(world, axis=0)) != len(world) or len(np.unique(pixels, axis=0)) != len(pixels):
        raise ValueError("중복된 대응점을 확인하세요.")
    center = world.mean(axis=0)
    scale = np.max(np.linalg.norm(world-center, axis=1))
    if scale < 1e-12:
        raise ValueError("올바른 대응점을 선택하세요.")
    x = (world-center)/scale
    _, singular, basis = np.linalg.svd(x, full_matrices=False)
    if singular[1] < 1e-3:
        raise ValueError("모델 점이 직선에 가깝습니다. 넓게 퍼진 점을 선택하세요.")
    planar = singular[2] < 1e-3
    if planar and estimate_principal:
        raise ValueError("평면의 점은 광학 중심 추정을 끄거나 다른 깊이의 점을 추가하세요.")
    principal = np.array([width/2, height/2])
    extent = max(width, height)
    rotations = []
    starts = []
    focals = [extent*.75, extent*1.5, extent*3, extent*6]
    if initial_camera:
        forward = np.asarray(initial_camera['direction'], dtype=float)
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, initial_camera['up']); right /= np.linalg.norm(right)
        rotations.append(np.array([right, -np.cross(right, forward), forward]))
    if planar:
        # IPPE returns both planar pose candidates at each focal seed. Flatten
        # only the initializer; refinement always uses the original 3D points.
        if np.linalg.det(basis) < 0: basis[2] *= -1
        plane = x@basis.T; plane[:,2] = 0
        for focal in focals:
            intrinsic = np.array([[focal,0,principal[0]],[0,focal,principal[1]],[0,0,1.]])
            _, rvecs, tvecs, _ = cv2.solvePnPGeneric(plane, pixels, intrinsic, None, flags=cv2.SOLVEPNP_IPPE)
            for rvec, tvec in zip(rvecs, tvecs):
                translation = tvec.ravel()
                if not np.isfinite(rvec).all() or not np.isfinite(translation).all() or translation[2] <= 1.011:
                    continue
                rotation = Rotation.from_rotvec(rvec.ravel()).as_matrix()@basis
                starts.append(np.r_[Rotation.from_matrix(rotation).as_rotvec(), translation[:2],
                                    np.log(translation[2]-1.01), np.log(focal)])
    else:
        # Normalized DLT seeds non-planar points. Both paths below refine a
        # centered principal point, square pixels and a proper rotation.
        normalized = (pixels-principal)/extent
        rows = []
        for point, (u, v) in zip(np.c_[x, np.ones(len(x))], normalized):
            rows.extend([np.r_[point, np.zeros(4), -u*point], np.r_[np.zeros(4), point, -v*point]])
        _, _, vt = np.linalg.svd(rows)
        projection = vt[-1].reshape(3, 4)
        if np.linalg.det(projection[:, :3]) < 0: projection = -projection
        intrinsic, rotation = rq(projection[:, :3])
        signs = np.diag(np.where(np.diag(intrinsic) < 0, -1., 1.))
        rotation = signs@rotation
        if np.linalg.det(rotation) > 0: rotations.append(rotation)
    if not rotations and not starts: rotations.append(np.eye(3))

    def project(parameters):
        rotation = Rotation.from_rotvec(parameters[:3]).as_matrix()
        translation = np.r_[parameters[3:5], 1.01+np.exp(parameters[5])]
        camera_points = x@rotation.T+translation
        return camera_points[:, :2]/camera_points[:, 2, None]*np.exp(parameters[6])+principal

    lower = [-np.inf]*5 + [np.log(.001), np.log(extent*.05)]
    upper = [np.inf]*5 + [np.log(1e7), np.log(extent*1e4)]
    for rotation in rotations:
        for focal in focals:
            depth = max(2., focal*2/max(np.ptp(pixels, axis=0).max(), 1))
            start = np.r_[Rotation.from_matrix(rotation).as_rotvec(), (pixels.mean(axis=0)-principal)*depth/focal,
                          np.log(depth-1.01), np.log(focal)]
            starts.append(start)
    fits = []
    for start in starts:
        result = least_squares(lambda v: (project(v)-pixels).ravel(), start,
                               bounds=(lower, upper), x_scale='jac', max_nfev=400,
                               ftol=1e-10, xtol=1e-10, gtol=1e-10)
        if result.success and np.isfinite(result.fun).all(): fits.append(result)
    if not fits: raise ValueError("카메라를 계산하지 못했습니다. 대응점을 확인하세요.")
    best = min(fits, key=lambda result: np.dot(result.fun, result.fun))
    if estimate_principal:
        # Keep square pixels and zero distortion fixed. The principal point is
        # expressed in the unchanged source image, including cropped composites.
        def shifted_project(parameters):
            focal = np.exp(parameters[6]); cx, cy = parameters[7:9]
            intrinsic = np.array([[focal, 0, cx], [0, focal, cy], [0, 0, 1.]])
            translation = np.r_[parameters[3:5], 1.01+np.exp(parameters[5])]
            return cv2.projectPoints(x, parameters[:3], translation, intrinsic, None)[0].reshape(-1, 2)
        best = least_squares(lambda v: (shifted_project(v)-pixels).ravel(), np.r_[best.x, principal],
                             bounds=(lower+[-2*extent]*2, upper+[3*extent]*2), x_scale='jac', max_nfev=2000,
                             ftol=1e-11, xtol=1e-11, gtol=1e-11)
        if not best.success: raise ValueError("카메라를 계산하지 못했습니다. 대응점을 확인하세요.")
        project = shifted_project
        principal = best.x[7:9]
    v = best.x; rotation = Rotation.from_rotvec(v[:3]).as_matrix()
    translation = np.r_[v[3:5], 1.01+np.exp(v[5])]
    focal = float(np.exp(v[6])); predicted = project(v)
    errors = np.linalg.norm(predicted-pixels, axis=1)
    normalized_jacobian = best.jac/np.maximum(np.linalg.norm(best.jac, axis=0), 1e-15)
    condition = float(np.linalg.cond(normalized_jacobian))
    stable = condition < 1e6 and lower[6]+.01 < v[6] < upper[6]-.01
    # A low residual alone cannot choose between distinct planar cameras.
    ambiguous = planar and any(
        np.sqrt(np.mean(candidate.fun**2)*2) <= np.sqrt(np.mean(errors**2))+.1 and
        ((Rotation.from_rotvec(candidate.x[:3])*Rotation.from_matrix(rotation).inv()).magnitude() > np.deg2rad(5) or
         abs(candidate.x[6]-v[6]) > np.log(1.2))
        for candidate in fits)
    stable = stable and not ambiguous
    if estimate_principal:
        stable = stable and np.all(principal > -2*extent+.01) and np.all(principal < 3*extent-.01)
    return {'camera': {'position': (center-rotation.T@translation*scale).tolist(),
                       'direction': rotation[2].tolist(), 'up': (-rotation[1]).tolist(),
                       'field_sdk': float(2*np.arctan(min(width, height)/(2*focal)))},
            'focal_px': focal, 'principal_px': principal.tolist(),
            'predicted_image_px': predicted.tolist(), 'point_errors_px': errors.tolist(),
            'max_error_px': float(errors.max()), 'rms_error_px': float(np.sqrt(np.mean(errors**2))),
            'stable': bool(stable), 'condition': condition, 'precision_passed': bool(stable and errors.max() <= .1 and not estimate_principal),
            'source': 'solver', 'estimate_principal': bool(estimate_principal), 'planar': bool(planar),
            'ambiguous': bool(ambiguous),
            'intrinsics_independently_validated': False,
            'model': 'perspective_estimated_principal_square_pixels_no_distortion' if estimate_principal else 'perspective_centered_principal_square_pixels_no_distortion',
            'coordinate_system': 'original_image_pixels'}
