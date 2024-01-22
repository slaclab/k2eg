import numpy as np
import time
from p4p.nt import NTTable, NTScalar, NTNDArray
from p4p.server import Server as PVAServer
from p4p.server.thread import SharedPV

# Example p4p server code modified from an original by Matt Gibbs

# Define the structure of the twiss table (what are the columns called, and what data type each column has)
twiss_table_type = NTTable([("element", "s"), ("device_name", "s"),
				 ("s", "d"), ("z", "d"), ("length", "d"), ("p0c", "d"),
				 ("alpha_x", "d"), ("beta_x", "d"), ("eta_x", "d"), ("etap_x", "d"), ("psi_x", "d"),
				 ("alpha_y", "d"), ("beta_y", "d"), ("eta_y", "d"), ("etap_y", "d"), ("psi_y", "d")])

# NOTE: p4p requires you to specify some initial value for every PV - there's not a default.
# here is where we make those default values.  This is a particularly hokey example.
twiss_table_rows = []
element_name_list = ["SOL9000", "XC99", "YC99"]
device_name_list = ["SOL:IN20:111", "XCOR:IN20:112", "YCOR:IN20:113"]
for i in range(0,len(element_name_list)):
	element_name = element_name_list[i]
	device_name = device_name_list[i]
	twiss_table_rows.append({"element": element_name, "device_name": device_name, "s": 0.0, "z": 0.0, "length": 0.0, "p0c": 6.0,
		"alpha_x": 0.0, "beta_x": 0.0, "eta_x": 0.0, "etap_x": 0.0, "psi_x": 0.0,
		"alpha_y": 0.0, "beta_y": 0.0, "eta_y": 0.0, "etap_y": 0.0, "psi_y": 0.0})

# Take the raw data and "wrap" it into the form that the PVA server needs.
initial_twiss_table = twiss_table_type.wrap(twiss_table_rows)

# Define a "handler" that gets called when somebody puts a value into a PV.
# In our case, this is sort of silly, because we just blindly dump whatever
# a client sends into the PV object.
class Handler(object):
	def put(self, pv, operation):
		try:
			pv.post(operation.value(), timestamp=time.time()) # just store the value and update subscribers
			operation.done()
		except Exception as e:
			operation.done(error=str(e))

# Define the PVs that will be hosted by the server.
live_twiss_pv = SharedPV(nt=twiss_table_type, initial=initial_twiss_table, handler=Handler())
image_pv = SharedPV(handler=Handler(), nt=NTNDArray(), initial=np.zeros(1))


# Make the PVA Server.  This is where we define the names for each of the PVs we defined above.
# By using "PVAServer.forever", the server immediately starts running, and doesn't stop until you
# kill it.
pva_server = PVAServer.forever(providers=[{f"K2EG:TEST:TWISS": live_twiss_pv, "K2EG:TEST:IMAGE": image_pv}])
