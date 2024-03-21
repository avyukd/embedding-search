#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "embedding_store.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;


PYBIND11_MODULE(embedding_search, m) {
    py::class_<EmbeddingStore> embedding_store(m, "EmbeddingStore");

    embedding_store.def(py::init(&EmbeddingStore::create))
        .def("addEmbedding", &EmbeddingStore::add_embedding)
        .def("getKClosest", &EmbeddingStore::get_k_closest);

    py::enum_<DistanceMetric>(embedding_store, "DistanceMetric")
        .value("cosine_similarity", DistanceMetric::cosine_similarity)
        .value("manhattan", DistanceMetric::manhattan)
        .value("l2_squared", DistanceMetric::l2_squared)
        .export_values();

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}

