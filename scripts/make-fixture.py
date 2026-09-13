"""Generate analytical reference pixels; never opens or changes an IronCAD scene."""
from pathlib import Path
import itertools, json, math, struct, zlib

root = Path(__file__).resolve().parents[1] / "tests" / "fixture"
root.mkdir(parents=True, exist_ok=True)
w, h, focal = 1600, 1000, 1000.0
camera = (350., -450., 350.)
direction = (-270., 500., -320.)
def unit(v):
    length = math.sqrt(sum(x*x for x in v))
    return tuple(x/length for x in v)
def cross(a,b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def dot(a,b):
    return sum(x*y for x,y in zip(a,b))
f = unit(direction)
r = unit(cross(f,(0,0,1)))
u = cross(r,f)
def project(p):
    q = tuple(x-y for x,y in zip(p,camera))
    depth = dot(q,f)
    assert depth > 0
    return (w/2 + focal*dot(q,r)/depth, h/2 - focal*dot(q,u)/depth)
boxes = [
    {"name":"A", "size":[100,80,60], "transform":"identity", "map":lambda x,y,z:(x,y,z)},
    {"name":"B", "size":[40,30,20],
     "transform":"inner: Ry(+90deg), translation(50,20,10); outer: Rz(+90deg), translation(180,60,0); column vectors; rotate then translate",
     "map":lambda x,y,z:(160-y,110+z,10-x)},
]
pixels = bytearray([245,245,245])*(w*h)
def pixel(x,y,color):
    x,y = int(round(x)),int(round(y))
    if 0 <= x < w and 0 <= y < h:
        k=(y*w+x)*3
        pixels[k:k+3]=bytes(color)
def line(a,b,color):
    dx,dy=b[0]-a[0],b[1]-a[1]
    n=max(1,int(math.ceil(max(abs(dx),abs(dy)))))
    for i in range(n+1): pixel(a[0]+dx*i/n,a[1]+dy*i/n,color)
records=[]
for box,color in zip(boxes,[(30,90,190),(220,100,25)]):
    vertices=[]
    for bits in itertools.product((0,1),repeat=3):
        local=tuple(bit*size for bit,size in zip(bits,box["size"]))
        world=box["map"](*local)
        uv=project(world)
        vertices.append((bits,uv))
        records.append({"label":box["name"]+"".join(map(str,bits)),"part":box["name"],"local":local,"world":world,"image_px":uv})
    for i,(bits,a) in enumerate(vertices):
        for other,b in vertices[i+1:]:
            if sum(x!=y for x,y in zip(bits,other))==1: line(a,b,color)
    for _,(x,y) in vertices:
        line((x-5,y),(x+5,y),(0,0,0));line((x,y-5),(x,y+5),(0,0,0))
# Border and image-center fiducials are independent of CAD geometry.
for a,b in [((0,0),(w-1,0)),((w-1,0),(w-1,h-1)),((w-1,h-1),(0,h-1)),((0,h-1),(0,0))]: line(a,b,(0,170,170))
line((w/2-10,h/2),(w/2+10,h/2),(150,150,150))
line((w/2,h/2-10),(w/2,h/2+10),(150,150,150))
def chunk(kind,data):
    return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data)&0xffffffff)
raw=b"".join(b"\0"+pixels[y*w*3:(y+1)*w*3] for y in range(h))
png=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(raw))+chunk(b"IEND",b"")
(root/"reference.png").write_bytes(png)
data={"status":"analytical_fixture_only_not_IronCAD_evidence","units":"arbitrary API units; determine millimeter conversion in the host first", "image_size":[w,h],"focal_px":focal,"camera":{"position":camera,"direction":direction,"up":[0,0,1]},"boxes":[{k:v for k,v in box.items() if k!="map"} for box in boxes],"vertices":records}
(root/"expected.json").write_text(json.dumps(data,indent=2)+"\n")
print("Wrote analytical reference.png and expected.json; no runtime pass claimed.")
