#pragma once

#include "pyl_Convert.h"

#include <memory>

#include <Python.h>

namespace Python
{
	// Deleter that calls Py_XDECREF on the PyObject parameter.
	struct PyObjectDeleter {
		void operator()(PyObject *obj) {
			Py_XDECREF(obj);
		}
	};
	// unique_ptr that uses Py_XDECREF as the destructor function.
	typedef std::unique_ptr<PyObject, PyObjectDeleter> pyunique_ptr;

	struct ExposedClass
	{
		std::vector<PyObject *> instances;
		std::string pyname;
		std::string classDef;
		ExposedClass(std::string n = " ", std::string d = "", std::vector<PyObject *> v = {});
	};

	// We need to keep the method definition's
	// string name and docs stored somewhere,
	// where their references are good, since they're char *
	// Also note! std::vectors can move in memory,
	// so when once this is exposed to python it shouldn't
	// be modified
	struct MethodDefinitions
	{
		// Method defs must be contiguous
		std::vector<PyMethodDef> v_Defs;

		// These containers don't invalidate references
		std::list<std::string> MethodNames, MethodDocs;

		// By default add the "null terminator",
		// all other methods are inserted before it
		MethodDefinitions() :
			v_Defs(1, { NULL, NULL, 0, })
		{}

		// Add method definitions before the null terminator
		size_t AddMethod(std::string name, PyCFunction fnPtr, int flags, std::string docs = "");

		// pointer to the definitions (which can move!)
		PyMethodDef * ptr() { return v_Defs.data(); }
	};

	// TODO more doxygen!
	// This is the original pywrapper::object... quite the beast
	/**
	* \class Object
	* \brief This class represents a python object.
	*/
	class Object {
	public:
		/**
		* \brief Constructs a default python object
		*/
		Object();

		/**
		* \brief Constructs a python object from a PyObject pointer.
		*
		* This Object takes ownership of the PyObject* argument. That
		* means no Py_INCREF is performed on it.
		* \param obj The pointer from which to construct this Object.
		*/
		Object(PyObject *obj);

		/**
		* \brief Calls the callable attribute "name" using the provided
		* arguments.
		*
		* This function might throw a std::runtime_error if there is
		* an error when calling the function.
		*
		* \param name The name of the attribute to be called.
		* \param args The arguments which will be used when calling the
		* attribute.
		* \return Python::Object containing the result of the function.
		*/
		template<typename... Args>
		Object call_function(const std::string &name, const Args&... args) {
			pyunique_ptr func(load_function(name));
			// Create the tuple argument
			pyunique_ptr tup(PyTuple_New(sizeof...(args)));
			add_tuple_vars(tup, args...);
			// Call our object
			PyObject *ret(PyObject_CallObject(func.get(), tup.get()));
			if (!ret)
				throw std::runtime_error("Failed to call function " + name);
			return{ ret };
		}

		/**
		* \brief Calls a callable attribute using no arguments.
		*
		* This function might throw a std::runtime_error if there is
		* an error when calling the function.
		*
		* \sa Python::Object::call_function.
		* \param name The name of the callable attribute to be executed.
		* \return Python::Object containing the result of the function.
		*/
		Object call_function(const std::string &name);

		/**
		* \brief Finds and returns the attribute named "name".
		*
		* This function might throw a std::runtime_error if an error
		* is encountered while fetching the attribute.
		*
		* \param name The name of the attribute to be returned.
		* \return Python::Object representing the attribute.
		*/
		Object get_attr(const std::string &name);

		/**
		* \brief Checks whether this object contains a certain attribute.
		*
		* \param name The name of the attribute to be searched.
		* \return bool indicating whether the attribute is defined.
		*/
		bool has_attr(const std::string &name);

		/**
		* \brief Returns the internal PyObject*.
		*
		* No reference increment is performed on the PyObject* before
		* returning it, so any DECREF applied to it without INCREF'ing
		* it will cause undefined behaviour.
		* \return The PyObject* which this Object is representing.
		*/
		PyObject *get() const { return py_obj.get(); }

		template<class T>
		bool convert(T &param) {
			return Python::convert(py_obj.get(), param);
		}

		/**
		* \brief Constructs a Python::Object from a script.
		*
		* The returned Object will be the representation of the loaded
		* script. If any errors are encountered while loading this
		* script, a std::runtime_error is thrown.
		*
		* \param script_path The path of the script to be loaded.
		* \return Object representing the loaded script.
		*/
		static Object from_script(const std::string &script_path);
	private:
		typedef std::shared_ptr<PyObject> pyshared_ptr;

		PyObject *load_function(const std::string &name);

		pyshared_ptr make_pyshared(PyObject *obj);

		// Variadic template method to add items to a tuple
		template<typename First, typename... Rest>
		void add_tuple_vars(pyunique_ptr &tup, const First &head, const Rest&... tail) {
			add_tuple_var(
				tup,
				PyTuple_Size(tup.get()) - sizeof...(tail)-1,
				head
				);
			add_tuple_vars(tup, tail...);
		}


		void add_tuple_vars(pyunique_ptr &tup, PyObject *arg) {
			add_tuple_var(tup, PyTuple_Size(tup.get()) - 1, arg);
		}

		// Base case for add_tuple_vars
		template<typename Arg>
		void add_tuple_vars(pyunique_ptr &tup, const Arg &arg) {
			add_tuple_var(tup,
				PyTuple_Size(tup.get()) - 1, alloc_pyobject(arg)
				);
		}

		// Adds a PyObject* to the tuple object
		void add_tuple_var(pyunique_ptr &tup, Py_ssize_t i, PyObject *pobj) {
			PyTuple_SetItem(tup.get(), i, pobj);
		}

		// Adds a PyObject* to the tuple object
		template<class T> void add_tuple_var(pyunique_ptr &tup, Py_ssize_t i,
			const T &data) {
			PyTuple_SetItem(tup.get(), i, alloc_pyobject(data));
		}

		pyshared_ptr py_obj;
	};
}