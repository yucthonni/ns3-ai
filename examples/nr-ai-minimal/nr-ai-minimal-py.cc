#include "nr-ai-minimal.h"
#include <ns3/ai-module.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

// Opaque declarations for vector interfaces
PYBIND11_MAKE_OPAQUE(ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::Cpp2PyMsgVector);
PYBIND11_MAKE_OPAQUE(ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::Py2CppMsgVector);

PYBIND11_MODULE(nr_ai_binding, m)
{
    py::class_<UeObservation>(m, "UeObservation")
        .def(py::init<>())
        .def_readwrite("rnti", &UeObservation::rnti)
        .def_readwrite("sinr", &UeObservation::sinr);

    py::class_<UeAction>(m, "UeAction")
        .def(py::init<>())
        .def_readwrite("txPower", &UeAction::txPower);

    // Bind the vector classes first
    // Use static_cast to disambiguate the resize function
    using Cpp2PyVec = ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::Cpp2PyMsgVector;
    py::class_<Cpp2PyVec>(m, "Cpp2PyMsgVector")
        .def("resize", static_cast<void (Cpp2PyVec::*)(Cpp2PyVec::size_type)>(&Cpp2PyVec::resize))
        .def("__len__", &Cpp2PyVec::size)
        .def("__getitem__", [](Cpp2PyVec& v, size_t i) -> UeObservation& { return v.at(i); }, py::return_value_policy::reference);

    using Py2CppVec = ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::Py2CppMsgVector;
    py::class_<Py2CppVec>(m, "Py2CppMsgVector")
        .def("resize", static_cast<void (Py2CppVec::*)(Py2CppVec::size_type)>(&Py2CppVec::resize))
        .def("__len__", &Py2CppVec::size)
        .def("__getitem__", [](Py2CppVec& v, size_t i) -> UeAction& { return v.at(i); }, py::return_value_policy::reference);

    // Bind the Interface implementation
    py::class_<ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>>(m, "Ns3AiMsgInterfaceImpl")
        .def(py::init<bool, bool, bool, uint32_t, const char*, const char*, const char*, const char*>())
        .def("PyRecvBegin", &ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::PyRecvBegin)
        .def("PyRecvEnd", &ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::PyRecvEnd)
        .def("PySendBegin", &ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::PySendBegin)
        .def("PySendEnd", &ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::PySendEnd)
        .def("PyGetFinished", &ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::PyGetFinished)
        .def("GetCpp2PyVector", &ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::GetCpp2PyVector, py::return_value_policy::reference)
        .def("GetPy2CppVector", &ns3::Ns3AiMsgInterfaceImpl<UeObservation, UeAction>::GetPy2CppVector, py::return_value_policy::reference);
}
