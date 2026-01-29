#include "nr-traces-ai.h"
#include <ns3/ai-module.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::Cpp2PyMsgVector);
PYBIND11_MAKE_OPAQUE(ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::Py2CppMsgVector);

PYBIND11_MODULE(nr_traces_ai_binding, m)
{
    py::class_<TraceObs>(m, "TraceObs")
        .def(py::init<>())
        .def_readwrite("rnti", &TraceObs::rnti)
        .def_readwrite("sinr", &TraceObs::sinr);

    py::class_<TraceAct>(m, "TraceAct")
        .def(py::init<>())
        .def_readwrite("txPower", &TraceAct::txPower);

    using Cpp2PyVec = ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::Cpp2PyMsgVector;
    py::class_<Cpp2PyVec>(m, "Cpp2PyMsgVector")
        .def("resize", static_cast<void (Cpp2PyVec::*)(Cpp2PyVec::size_type)>(&Cpp2PyVec::resize))
        .def("__len__", &Cpp2PyVec::size)
        .def("__getitem__", [](Cpp2PyVec& v, size_t i) -> TraceObs& { return v.at(i); }, py::return_value_policy::reference);

    using Py2CppVec = ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::Py2CppMsgVector;
    py::class_<Py2CppVec>(m, "Py2CppMsgVector")
        .def("resize", static_cast<void (Py2CppVec::*)(Py2CppVec::size_type)>(&Py2CppVec::resize))
        .def("__len__", &Py2CppVec::size)
        .def("__getitem__", [](Py2CppVec& v, size_t i) -> TraceAct& { return v.at(i); }, py::return_value_policy::reference);

    py::class_<ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>>(m, "Ns3AiMsgInterfaceImpl")
        .def(py::init<bool, bool, bool, uint32_t, const char*, const char*, const char*, const char*>())
        .def("PyRecvBegin", &ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::PyRecvBegin)
        .def("PyRecvEnd", &ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::PyRecvEnd)
        .def("PySendBegin", &ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::PySendBegin)
        .def("PySendEnd", &ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::PySendEnd)
        .def("PyGetFinished", &ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::PyGetFinished)
        .def("GetCpp2PyVector", &ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::GetCpp2PyVector, py::return_value_policy::reference)
        .def("GetPy2CppVector", &ns3::Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>::GetPy2CppVector, py::return_value_policy::reference);
}
