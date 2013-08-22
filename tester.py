import zmq

class JRPC:
	def __init__(self):
		self.id = 0

	def make_req(self, method, params):
		req = {"jsonrpc":"2.0", "method":method, "params":params,
				"id":self.id}
		self.id += 1
		return req

zctx = zmq.Context.instance()
zsock = zctx.socket(zmq.REQ)
zsock.connect("tcp://127.0.0.1:10000")

jrpc = JRPC()

req = jrpc.make_req("echo", [10, 5])
zsock.send_json(req)
print zsock.recv()

req = jrpc.make_req("subtract", {"minuend":10, "subtrahend":5})
zsock.send_json(req)
print zsock.recv()

req = jrpc.make_req("subtract", [10, 5])
zsock.send_json(req)
print zsock.recv()

req_array = []
for k in range(10):
	req = jrpc.make_req("sum", range(1+k))
	req_array.append(req)
zsock.send_json(req_array)
print zsock.recv()

