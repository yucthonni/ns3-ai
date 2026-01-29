import sys
import nr_sionna_ai_binding as py_binding
from ns3ai_utils import Experiment

# 配置实验环境
# 脚本在 contrib/ai/examples/nr-sionna-ai/，ns3根目录在 ../../../../
exp = Experiment("nr-sionna-ai", "../../../../", py_binding, 
                 handleFinish=True, useVector=True, vectorSize=1)

msg = exp.run(show_output=True)
print("Python: Sionna-NR-AI loop starting...")

try:
    while True:
        msg.PyRecvBegin()
        if msg.PyGetFinished(): break
        
        obs = msg.GetCpp2PyVector()[0]
        # 根据 SINR 做出决策
        new_tp = 23.0 if obs.sinr < 12.0 else 10.0
        
        msg.PySendBegin()
        msg.GetPy2CppVector()[0].txPower = new_tp
        msg.PySendEnd()
        msg.PyRecvEnd()
except Exception as e:
    print(f"Error: {e}")
finally:
    del exp
