import nr_traces_ai_binding as py_binding
from ns3ai_utils import Experiment
import sys

# 初始化实验环境
exp = Experiment("nr-traces-ai", "../../../../", py_binding, 
                 handleFinish=True, useVector=True, vectorSize=1)

msg = exp.run(show_output=True)
print("Python: Trace-NR-AI loop starting...")

try:
    while True:
        # 同步接收
        msg.PyRecvBegin()
        if msg.PyGetFinished(): break
        
        obs = msg.GetCpp2PyVector()[0]
        # print(f"Python: Obs -> RNTI={obs.rnti}, SINR={obs.sinr:.2f}")

        # 简单动作：根据实时信道质量调整功率
        new_power = 23.0 if obs.sinr < 15.0 else 10.0
        
        # 同步发送
        msg.PySendBegin()
        msg.GetPy2CppVector()[0].txPower = new_power
        msg.PySendEnd()
        msg.PyRecvEnd()
except Exception as e:
    print(f"Error: {e}")
finally:
    del exp
