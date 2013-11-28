import zmq

class JRPC:
	def __init__(self):
		self.id = 0

	def make_noti(self, method, params=None):
		noti = {"jsonrpc":"2.0", "method":method}
		if params is not None:
			noti["params"] = params
		return noti

	def make_req(self, method, params=None):
		req = self.make_noti(method, params)
		req["id"] = self.id
		self.id += 1
		return req

zctx = zmq.Context.instance()
zsock = zctx.socket(zmq.REQ)
zsock.connect("tcp://127.0.0.1:10000")

jrpc = JRPC()

# test "echo" method
req = jrpc.make_req("echo", [10, 5])
zsock.send_json(req)
rep = zsock.recv_json()
assert(rep['result']==req['params'])

# test "counter" method and batch
req = jrpc.make_req("counter")
zsock.send_json([req]*10)
batchrep = zsock.recv_json()
counts = [rep['result'] for rep in batchrep]
for k in range(1,len(counts)):
	assert counts[k] - counts[k-1] == 1

# test "sum" method and batch
batchreq = []
for k in range(10):
	batchreq.append(jrpc.make_req("sum", range(1+k)))
zsock.send_json(batchreq)
batchrep = zsock.recv_json()
for k in range(10):
	assert(batchrep[k]['result']==sum(range(1+k)))

a = range(3)
o = {1:1, 2:2, 3:3}

d = { "one": "un", "two": 2, "three": 3.0, "four": True, "five": False, "six": None, "seven":a, "eight":o }
req = jrpc.make_noti("iterate", d)
zsock.send_json(req)
rep = zsock.recv()
assert not rep

req = jrpc.make_noti("iterate", a)
zsock.send_json(req)
rep = zsock.recv()
assert not rep

req = jrpc.make_noti("foreach", d)
zsock.send_json(req)
rep = zsock.recv()
assert not rep

req = jrpc.make_noti("foreach", a)
zsock.send_json(req)
rep = zsock.recv()
assert not rep


