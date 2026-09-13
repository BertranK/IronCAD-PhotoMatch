"""Production transport against a native echo harness; not an IronCAD API test."""
from pathlib import Path
import subprocess
import sys
import unittest
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'gui'))
from bridge import Bridge, BridgeError


class TransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        executable=Path(__file__).resolve().parents[1]/'build/v143/PipeHarness.exe'
        cls.process=subprocess.Popen([str(executable)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        cls.host=int(cls.process.stdout.readline())

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate();cls.process.communicate(timeout=5)

    def test_reconnect_unicode_and_message_boundaries(self):
        bridge=Bridge(self.host)
        for i in range(12):
            expected={'sequence':i,'name':'레고 사진 α','position':[.125,-.762,.508]}
            self.assertEqual(expected,bridge.call('echo',args=expected))

    def test_independent_clients(self):
        for i in range(5):
            self.assertEqual({'client':i},Bridge(self.host).call('echo',args={'client':i}))

    def test_simultaneous_clients_do_not_lose_connection_race(self):
        def request(i):
            return Bridge(self.host).call('echo',args={'client':i})
        with ThreadPoolExecutor(max_workers=8) as pool:
            results=list(pool.map(request,range(32)))
        self.assertEqual([{'client':i} for i in range(32)],results)

    def test_large_response(self):
        self.assertEqual('x'*200000,Bridge(self.host).call('echo',args={'large':True})['payload'])

    def test_client_timeout_does_not_break_next_connection(self):
        with self.assertRaises(BridgeError):
            Bridge(self.host,50).call('echo',args={'delay':200})
        self.assertEqual({'after_timeout':True},Bridge(self.host).call('echo',args={'after_timeout':True}))


if __name__=='__main__':unittest.main()
