from setuptools import setup, find_packages

setup(
    name="ns3ai_gym_env",
    version="0.0.1",
    packages=find_packages(),
    py_modules=['messages_pb2'],  # 关键：将 messages_pb2 作为顶级模块
    package_dir={
        'ns3ai_gym_env': 'ns3ai_gym_env',
        '': '.',  # 将当前目录也作为包目录
    },
    install_requires=["numpy", "gymnasium", "protobuf"],
)