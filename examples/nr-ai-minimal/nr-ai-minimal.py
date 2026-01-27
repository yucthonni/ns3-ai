import sys
import os
import nr_ai_binding as py_binding
from ns3ai_utils import Experiment
import random

# Define experiment name - this should match the executable target name
executable_name = "nr-ai-minimal" 

# Root of ns-3 relative to this script
ns3_root = "../../../../"

# Create Experiment
exp = Experiment(executable_name, ns3_root, py_binding, 
                 handleFinish=True, useVector=True, vectorSize=1)

print("Python: Waiting for C++ connection...")
msg = exp.run(show_output=True)

print("Python: Connection established!")

try:
    while True:
        # 1. Receive Observation
        msg.PyRecvBegin()
        if msg.PyGetFinished():
            print("Python: Simulation finished.")
            break
        
        obs = msg.GetCpp2PyVector()[0]
        # print(f"Python: Obs -> RNTI={obs.rnti}, SINR={obs.sinr}")

        # 2. Compute Action
        msg.PySendBegin()
        act = msg.GetPy2CppVector()[0]
        act.txPower = random.randint(0, 23)  # Random txPower between 0 and 23 dBm
            
        # 3. Send Action
        msg.PySendEnd()
        msg.PyRecvEnd()

except Exception as e:
    print(f"Python Error: {e}")
finally:
    del exp
