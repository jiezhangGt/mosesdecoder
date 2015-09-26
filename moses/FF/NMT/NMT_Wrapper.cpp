#include "NMT_Wrapper.h"

#include "util/exception.hh"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <streambuf>
#include <assert.h>


using namespace std;
using namespace util;


NMT_Wrapper::NMT_Wrapper()
{
    py_get_log_prob = PyString_FromString((char*)"get_log_prob");
    py_get_log_probs = PyString_FromString((char*)"get_log_probs");
    py_get_vec_log_probs = PyString_FromString((char*)"get_vec_log_probs");
    py_get_context_vectors = PyString_FromString((char*)"get_context_vector");
    py_get_next_states = PyString_FromString((char*)"get_next_states");
    py_get_log_prob_states = PyString_FromString((char*)"get_log_prob_states");
}

bool NMT_Wrapper::GetContextVectors(const string& source_sentence, PyObject*& vectors)
{
    PyObject* py_source_sentence = PyString_FromString(source_sentence.c_str());
    vectors = PyObject_CallMethodObjArgs(py_wrapper, py_get_context_vectors, py_source_sentence, NULL);
    return true;
}

void NMT_Wrapper::AddPathToSys(const string& path)
{
    PyObject* py_sys_path = PySys_GetObject((char*)"path");
    PyList_Append(py_sys_path, PyString_FromString(path.c_str()));
}


bool NMT_Wrapper::Init(const string& state_path, const string& model_path, const string& wrapper_path)
{

    this->state_path = state_path;
    this->model_path = model_path;

    Py_Initialize();
    PyErr_Print();

    AddPathToSys(wrapper_path);

    PyObject* filename = PyString_FromString((char*) "nmt_wrapper");
    PyObject* imp = PyImport_Import(filename);
    if (imp == NULL) {
        cerr << "No import\n"; return false;
    }

    PyObject* wrapper_name = PyObject_GetAttrString(imp, (char*)"NMTWrapper");
    if (wrapper_name == NULL) {
        cerr << "No wrapper\n"; return false;
    }

    PyObject* args = PyTuple_Pack(2, PyString_FromString(state_path.c_str()), PyString_FromString(model_path.c_str()));
    py_wrapper = PyObject_CallObject(wrapper_name, args);
    if (py_wrapper == NULL) {
        return false;
    }

    if (PyObject_CallMethod(py_wrapper, (char*)"build", NULL) == NULL) {
        return false;
    }


    return true;
}

bool NMT_Wrapper::GetProb(const string& next_word,
                          PyObject* py_context_vectors,
                          const string& last_word,
                          PyObject* input_state,
                          double& output_prob,
                          PyObject*& output_state)
{
    PyObject* py_next_word = PyString_FromString(next_word.c_str());
    PyObject* py_response = NULL;

    if (input_state == NULL)
    {
        py_response = PyObject_CallMethodObjArgs(py_wrapper, py_get_log_prob, py_next_word, py_context_vectors, NULL);
    }
    else {
        PyObject* py_last_word = PyString_FromString(last_word.c_str());
        py_response = PyObject_CallMethodObjArgs(py_wrapper, py_get_log_prob, py_next_word, py_context_vectors,
                                                 py_last_word, input_state, NULL);
    }

   if (py_response == NULL) { return false; }
    if (! PyTuple_Check(py_response)) { return false; }

    PyObject* py_prob = PyTuple_GetItem(py_response, 0);
    if (py_prob == NULL) { return false; }
    output_prob = PyFloat_AsDouble(py_prob);

    output_state = PyTuple_GetItem(py_response, 1);
    if (output_state == NULL) { return 0; }

    return true;
}

bool NMT_Wrapper::GetProb(const std::vector<std::string>& next_words,
                          PyObject* py_context_vectors,
                          const string& last_word,
                          PyObject* input_state,
                          double& logProb,
                          PyObject*& output_state)
{
    PyObject* py_nextWords = PyList_New(0);
    for (size_t i = 0; i < next_words.size(); ++i) {
        PyList_Append(py_nextWords, PyString_FromString(next_words[i].c_str()));
    }

    PyObject* py_response = NULL;

    if (input_state == NULL)
    {
        py_response = PyObject_CallMethodObjArgs(py_wrapper, py_get_log_probs,
                                                 py_nextWords, py_context_vectors, NULL);
    }
    else {
        PyObject* py_last_word = PyString_FromString(last_word.c_str());
        py_response = PyObject_CallMethodObjArgs(py_wrapper, py_get_log_probs, py_nextWords, py_context_vectors,
                                                 py_last_word, input_state, NULL);
    }

    if (py_response == NULL) { return false; }
    if (! PyTuple_Check(py_response)) { return false; }

    PyObject* py_prob = PyTuple_GetItem(py_response, 0);
    if (py_prob == NULL) { return false; }
    logProb = PyFloat_AsDouble(py_prob);

    output_state = PyTuple_GetItem(py_response, 1);
    if (output_state == NULL) { return 0; }

    return true;
}
void ParseWrapperResponse(
        PyObject*& pyResponse,
        std::vector< std::vector< double > >& logProbs,
        std::vector< std::vector< PyObject* > >& outputStates)
{

}


bool NMT_Wrapper::GetProb(const std::vector<std::string>& nextWords,
                          PyObject* pyContextVectors,
                          const std::vector< string >& lastWords,
                          std::vector< PyObject* >& inputStates,
                          std::vector< std::vector< double > >& logProbs,
                          std::vector< std::vector< PyObject* > >& outputStates)
{
    // UTIL_THROW_IF2(lastWords.size() != inputStates.size(), "#lastWords != #inputStates");
    PyObject* pyNextWords = PyList_New(0);
    for (size_t i = 0; i < nextWords.size(); ++i) {
        PyList_Append(pyNextWords, PyString_FromString(nextWords[i].c_str()));
    }

    PyObject* pyLastWords = PyList_New(0);
    for (size_t i = 0; i < lastWords.size(); ++i) {
        PyList_Append(pyLastWords, PyString_FromString(lastWords[i].c_str()));
    }

    PyObject* pyResponse = NULL;
    if (inputStates.size() == 0) {
        pyResponse = PyObject_CallMethodObjArgs(py_wrapper,
                                                py_get_vec_log_probs,
                                                pyNextWords,
                                                pyContextVectors,
                                                NULL);
    } else {
        PyObject* pyInputStates = PyList_New(0);
        for (size_t i = 0; i < inputStates.size(); ++i) {
            PyList_Append(pyInputStates, inputStates[i]);
        }

        PyObject* pyLastWords = PyList_New(0);
        for (size_t i = 0; i < lastWords.size(); ++i) {
            PyList_Append(pyLastWords, PyString_FromString(lastWords[i].c_str()));
        }

        pyResponse = PyObject_CallMethodObjArgs(py_wrapper,
                                                py_get_vec_log_probs,
                                                pyNextWords,
                                                pyContextVectors,
                                                pyLastWords,
                                                pyInputStates,
                                                NULL);
    }
    UTIL_THROW_IF2(pyResponse == NULL, "No response from python module.");

    size_t inputSize = 0;
    if (inputStates.size() == 0) {
        inputSize = 1;
    } else {
        inputSize = inputStates.size();
    }

    PyObject* pyLogProbMatrix = PyTuple_GetItem(pyResponse, 0);
    PyObject* pyOutputStateMatrix = PyTuple_GetItem(pyResponse, 1);
    logProbs.clear();
    outputStates = vector<vector<PyObject*> >(inputSize, vector<PyObject*>(nextWords.size(), NULL));
    vector<double> hipoProbs;
    vector<PyObject*> hipoStates;
    logProbs.clear();
    for (size_t i = 0; i < inputSize; ++i) {
        hipoProbs.clear();
        hipoStates.clear();

        PyObject* pyLogProbColumn = PyList_GetItem(pyLogProbMatrix, i);
        for (size_t j = 0; j < nextWords.size(); ++j) {
            hipoProbs.push_back(PyFloat_AsDouble(PyList_GetItem(pyLogProbColumn, j)));
        }
        logProbs.push_back(hipoProbs);
    }

    for (size_t j = 0; j < nextWords.size(); ++j) {
        PyObject* pyOutputStateColumn = PyList_GetItem(pyOutputStateMatrix, j);
        for (size_t i = 0; i < inputSize; ++i) {
            outputStates[i][j]  = PyList_GetItem(pyOutputStateColumn, i);
        }

        outputStates.push_back(hipoStates);
    }


    cerr << "Wychodze z GetProb!" << endl;
    return true;
}
void NMT_Wrapper::GetNextStates(
        const std::vector<std::string>& nextWords,
        PyObject* pyContextVectors,
        std::vector<PyObject*>& inputStates,
        std::vector<PyObject*>& nextStates)
{
    UTIL_THROW_IF2(nextWords.size() != nextStates.size(), "#nextWords != #inputStates");
    UTIL_THROW_IF2(nextWords.size() == 0, "No hipothesis!");

    PyObject* pyNextWords = PyList_New(0);
    for (size_t i = 0; i < nextWords.size(); ++i) {
        PyList_Append(pyNextWords, PyString_FromString(nextWords[i].c_str()));
    }


    PyObject* pyInputStates = PyList_New(0);
    for (size_t i = 0; i < inputStates.size(); ++i) {
        PyList_Append(pyInputStates, inputStates[i]);
    }

    PyObject* pyResponse = PyObject_CallMethodObjArgs(py_wrapper,
                                            py_get_next_states,
                                            pyNextWords,
                                            pyContextVectors,
                                            pyInputStates,
                                            NULL);

    UTIL_THROW_IF2(pyResponse == NULL, "No response from python module.");

    nextStates.clear();
    size_t nextStatesSize = PyList_Size(pyResponse);
    UTIL_THROW_IF2(nextStatesSize != inputStates.size(), "Returned bad number of states.");
    for (size_t i = 0; i < nextStatesSize; ++i) {
        nextStates.push_back(PyList_GetItem(pyResponse, i));
    }
}
void NMT_Wrapper::GetNextLogProbStates(
    const std::vector<std::string>& nextWords,
    PyObject* pyContextVectors,
    const std::vector< std::string >& lastWords,
    std::vector<PyObject*>& inputStates,
    std::vector<double>& logProbs,
    std::vector<PyObject*>& nextStates)
{
    UTIL_THROW_IF2(lastWords.size() != inputStates.size(), "#lastWords != #inputStates");

    PyObject* pyNextWords = PyList_New(0);
    for (size_t i = 0; i < nextWords.size(); ++i) {
        PyList_Append(pyNextWords, PyString_FromString(nextWords[i].c_str()));
    }

    PyObject* pyLastWords = PyList_New(0);
    for (size_t i = 0; i < lastWords.size(); ++i) {
        PyList_Append(pyLastWords, PyString_FromString(lastWords[i].c_str()));
    }

    PyObject* pyResponse = NULL;
    PyObject* pyStates = PyList_New(0);
    for (size_t i = 0; i < inputStates.size(); ++i) {
        if (PyList_Append(pyStates, inputStates[i]) == -1); // cerr << i << ": "<< "ARGH! " << inputStates[i] << endl;
    }
    if (inputStates.size() == 0) {
        pyResponse = PyObject_CallMethodObjArgs(py_wrapper,
                                                py_get_log_prob_states,
                                                pyNextWords,
                                                pyContextVectors,
                                                NULL);
    } else {
        //cerr << "A: " << PyList_Size(pyStates) << endl;
        pyResponse = PyObject_CallMethodObjArgs(py_wrapper,
                                                py_get_log_prob_states,
                                                pyNextWords,
                                                pyContextVectors,
                                                pyLastWords,
                                                pyStates,
                                                NULL);
    }
    UTIL_THROW_IF2(pyResponse == NULL, "No response from python module.");

    size_t inputSize = 0;
    if (inputStates.size() == 0) {
        inputSize = 1;
    } else {
        inputSize = inputStates.size();
    }

    PyObject* pyLogProbs = PyTuple_GetItem(pyResponse, 0);
    PyObject* pyNextStates = PyTuple_GetItem(pyResponse, 1);
    logProbs.clear();
    nextStates.clear();

    for (size_t j = 0; j < inputSize; ++j) {
        logProbs.push_back(PyFloat_AsDouble(PyList_GetItem(pyLogProbs, j)));
    }
    //cerr << "S: " << PyList_Size(pyNextStates) << endl;
    for (size_t i = 0; i < inputSize; ++i) {
        PyObject* nextState = PyList_GetItem(pyNextStates, i);
        if (nextState == NULL) cerr << "NULL OUTOUT" << endl;
        nextStates.push_back(nextState);
    }
    for(size_t i = 0; i < nextStates.size(); ++i) {
        // cerr << "STATES: " << nextStates[i] << endl;
    }

    //cerr << "Wychodze z GetLogProbStates!" << endl;
    
    
}

NMT_Wrapper::~NMT_Wrapper()
{
    Py_Finalize();
}

