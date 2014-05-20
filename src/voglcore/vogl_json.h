/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

// File: vogl_json.h
//
// TODO: Ensure UBJ format is compatible with the latest specification. I'm using the spec from late 2011.
//
#pragma once

#include "vogl_core.h"
#include "vogl_strutils.h"
#include "vogl_vec.h"
#include "vogl_object_pool.h"

#define VOGL_TEXT_JSON_EXTENSION "json"
#define VOGL_BINARY_JSON_EXTENSION "ubj"

namespace vogl
{

    class json_growable_char_buf;
    class json_deserialize_buf_ptr;
    class json_document;

    template <typename T, uint32_t N>
    class growable_array;

    template <typename Key, typename Value, typename Hasher, typename Equals>
    class hash_map;

    template <typename Key, typename Value, typename Hasher, typename Equals, typename Allocator>
    class rh_hash_map;

    template <typename Key, typename Value, typename LessComp, typename EqualComp, uint32_t MaxLevels>
    class map;

    enum json_value_type_t
    {
        cJSONValueTypeNull = 0,
        cJSONValueTypeBool,
        cJSONValueTypeInt, // 64-bit signed integer
        cJSONValueTypeDouble,
        cJSONValueTypeString,
        cJSONValueTypeNode // A node is a JSON object or array.
    };

    enum
    {
        CMaxLineLenDefault = 160
    };

    class json_node;

    // A simple value variant.
    union json_value_data_t
    {
        int64_t m_nVal;
        double m_flVal;
        json_node *m_pNode;
        char *m_pStr;
    };

    // Parsing error coordinate.
    struct json_error_info_t
    {
        int m_error_line;
        dynamic_string m_error_msg;

        void set_error(uint32_t line, const char *pMsg, ...) VOGL_ATTRIBUTE_PRINTF(3, 4);
    };

    // Node pool
    typedef object_pool<json_node, object_pool_spinlock_locking_policy> json_node_object_pool;
    extern json_node_object_pool *g_pJSON_node_pool;

    void json_node_pool_init();

    inline json_node_object_pool *get_json_node_pool()
    {
        if (!g_pJSON_node_pool)
            json_node_pool_init();
        return g_pJSON_node_pool;
    }

    // A json_value is a null, bool, int64_t, double, a heap pointer to a null terminated string, or a heap pointer to a json_node (which is either a object or array).
    // A json_value owns any string/node it points to, so the referenced strings/json_node is deleted upon destruction.
    class json_value
    {
        friend class json_node;

    public:
        inline json_value();
        inline json_value(bool val);
        inline json_value(int32_t nVal);
        inline json_value(uint32_t nVal);
        inline json_value(int64_t nVal);
        // Note uint64_t values may be encoded as hex strings or int64_t
        inline json_value(uint64_t nVal);
        inline json_value(double flVal);
        inline json_value(char *pStr);
        inline json_value(const char *pStr);
        inline json_value(const dynamic_string &str);
        inline json_value(const json_node *pNode);
        inline json_value(json_value_type_t type);
        inline ~json_value();

        json_value(const json_value &other);
        json_value &operator=(const json_value &rhs);

        inline json_value_type_t get_type() const;

        inline const json_value_data_t &get_raw_data() const;

        json_value_data_t &get_raw_data();

        inline bool is_null() const;
        inline bool is_valid() const;
        inline bool is_bool() const;
        inline bool is_int() const;
        inline bool is_double() const;
        inline bool is_numeric() const;
        inline bool is_string() const;
        inline bool is_node() const;
        inline bool is_object_or_array() const;
        inline bool is_object() const;
        inline bool is_array() const;

        inline const json_node *get_node_ptr() const;

        inline json_node *get_node_ptr();

        inline void clear();

        inline void assume_ownership(json_value &src_val);

        inline void release_ownership(json_value &dst_value);

        inline void init(json_value_type_t type);
        inline json_node *init_object();
        inline json_node *init_array();

        inline json_node *set_value_to_object(json_node *pParent = NULL);
        inline json_node *set_value_to_array(json_node *pParent = NULL);
        inline json_node *set_value_to_node(bool is_object, json_node *pParent = NULL);

        inline void set_value_to_null();
        inline void set_value(bool val);
        inline void set_value(int8_t nVal);
        inline void set_value(int16_t nVal);
        inline void set_value(int32_t nVal);
        inline void set_value(int64_t nVal);

        inline void set_value(uint8_t nVal);
        inline void set_value(uint16_t nVal);
        inline void set_value(uint32_t nVal);

        // Note uint64_t values may be encoded as hex strings or int64_t
        void set_value(uint64_t nVal);

        inline void set_value(double flVal);
        inline void set_value(const char *pStr);
        inline void set_value(const dynamic_string &str);

        // copies contents of *pNode into a new node, sets value to point to new node.
        inline void set_value(const json_node *pNode);

        inline void set_value_assume_ownership(char *pStr);
        inline void set_value_assume_ownership(json_node *pNode);

        inline json_value &operator=(bool val);
        inline json_value &operator=(int32_t nVal);
        inline json_value &operator=(uint32_t nVal);
        inline json_value &operator=(int64_t nVal);
        inline json_value &operator=(uint64_t nVal);
        inline json_value &operator=(double flVal);
        inline json_value &operator=(const char *pStr);

        // Attempts to convert a JSON value to each type.
        // Returns false and sets val to def if the value cannot be converted (out of range, or type is obviously incompatible).
        // Note that if trying to convert a negative value to an unsigned type, or a value which is too large will fail and you'll get the default.
        inline bool get_bool(bool &val, bool def = false) const;
        bool get_numeric(int8_t &val, int8_t def = 0) const;
        bool get_numeric(int16_t &val, int16_t def = 0) const;
        inline bool get_numeric(int32_t &val, int32_t def = 0) const;
        inline bool get_numeric(int64_t &val, int64_t def = 0) const;

        bool get_numeric(uint8_t &val, uint8_t def = 0) const;
        bool get_numeric(uint16_t &val, uint16_t def = 0) const;
        bool get_numeric(uint32_t &val, uint32_t def = 0) const;
        inline bool get_numeric(uint64_t &val, uint64_t def = 0) const;

        inline bool get_numeric(float &val, float def = 0.0f) const;
        inline bool get_numeric(double &val, double def = 0.0f) const;

        inline bool get_string(dynamic_string &val, const char *pDef = "") const;
        bool get_enum(const char **pStringList, int &val, int def) const;

        inline bool as_bool(bool def = false) const;
        inline int as_int(int def = 0) const;
        inline int as_int32(int32_t def = 0) const;
        inline uint32_t as_uint32(uint32_t def = 0) const;
        inline int64_t as_int64(int64_t def = 0) const;
        inline uint64_t as_uint64(uint64_t def = 0) const;
        inline float as_float(float def = 0.0f) const;
        inline double as_double(double def = 0.0f) const;

        // Returns value as a string, or the default string if the value cannot be converted.
        inline dynamic_string as_string(const char *pDef = "") const;

        // Returns pointer to null terminated string or NULL if the value is not a string.
        inline const char *as_string_ptr(const char *pDef = NULL) const;

        inline bool operator==(const json_value &other) const;
        inline bool operator!=(const json_value &other) const;

        inline void swap(json_value &other);

        // Deserialize value from a UTF8 file, null terminated string or buffer.
        bool deserialize_file(const char *pFilename, json_error_info_t *pError_info = NULL);
        bool deserialize(FILE *pFile, json_error_info_t *pError_info = NULL);
        bool deserialize(const vogl::vector<char> &buf, json_error_info_t *pError_info = NULL);
        bool deserialize(const char *pStr, json_error_info_t *pError_info = NULL);
        bool deserialize(const dynamic_string &str, json_error_info_t *pError_info = NULL);
        bool deserialize(const char *pBuf, size_t buf_size, json_error_info_t *pError_info = NULL);

        // Serialize to a UTF8 file, a growable buffer, or a string
        bool serialize_to_file(const char *pFilename, bool formatted = true, uint32_t max_line_len = CMaxLineLenDefault) const;
        bool serialize(FILE *pFile, bool formatted = true, uint32_t max_line_len = CMaxLineLenDefault) const;
        void serialize(vogl::vector<char> &buf, bool formatted = true, uint32_t cur_indent = 0, bool null_terminate = true, uint32_t max_line_len = CMaxLineLenDefault) const;
        // Serializes to a dynamic_string
        dynamic_string &serialize(dynamic_string &str, bool formatted = true, uint32_t cur_indent = 0, uint32_t max_line_len = CMaxLineLenDefault) const;
        // Serialize to stdout.
        void print(bool formatted = true, uint32_t cur_indent = 0, uint32_t max_line_len = CMaxLineLenDefault) const;

        // Deserialize from Universal Binary JSON (UBJ).
        bool binary_deserialize(const uint8_t *pBuf, size_t buf_size);
        bool binary_deserialize_file(const char *pFilename);
        bool binary_deserialize(FILE *pFile);
        bool binary_deserialize(const vogl::vector<uint8_t> &buf);

        // Serialize from UBJ.
        void binary_serialize(vogl::vector<uint8_t> &buf) const;
        bool binary_serialize_to_file(const char *pFilename);
        bool binary_serialize(FILE *pFile);

        bool is_equal(const json_value &other, double tol = 1e-8) const;

        // Returns true if all parent pointers are correct.
        bool validate(const json_node *pParent = NULL) const;

        void set_line(uint32_t line);

        uint32_t get_line() const;

    protected:
        json_value_data_t m_data;
        json_value_type_t m_type;
        uint32_t m_line;

        bool convert_to_bool(bool &val, bool def) const;
        bool convert_to_int32(int32_t &val, int32_t def) const;
        bool convert_to_int64(int64_t &val, int64_t def) const;
        bool convert_to_uint64(uint64_t &val, uint64_t def) const;
        bool convert_to_float(float &val, float def) const;
        bool convert_to_double(double &val, double def) const;
        bool convert_to_string(dynamic_string &val, const char *pDef) const;

        inline void free_data()
        {
            if (m_type == cJSONValueTypeString)
                vogl_free(m_data.m_pStr);
            else if (m_type == cJSONValueTypeNode)
                get_json_node_pool()->destroy(m_data.m_pNode);
        }

        bool deserialize_node(json_deserialize_buf_ptr &pStr, json_node *pParent, uint32_t level, json_error_info_t &error_info);
        bool estimate_deserialized_string_size(json_deserialize_buf_ptr &pStr, json_error_info_t &error_info, uint32_t &size);
        bool deserialize_quoted_string_to_buf(char *&pBuf, uint32_t buf_size, json_deserialize_buf_ptr &pStr, json_error_info_t &error_info);
        bool deserialize_string(json_deserialize_buf_ptr &pStr, json_error_info_t &error_info);
        bool deserialize_number(json_deserialize_buf_ptr &pStr, json_error_info_t &error_info);
        bool deserialize(json_deserialize_buf_ptr &pStr, json_node *pParent, uint32_t level, json_error_info_t &error_info);

        void binary_serialize_value(vogl::vector<uint8_t> &buf) const;
        bool binary_deserialize(const uint8_t *&pBuf, const uint8_t *pBuf_end, json_node *pParent, uint32_t depth);

    private:
        // Optional: Forbid (the super annoying) implicit conversions of all pointers/const pointers to bool, except for the specializations below.
        // Purposely unimplemented. Constructor will be caught at compile time, and operator= at link time.
        // TODO: Is this confusing crap needed anymore?
        template <typename T>
        json_value(T *);
        template <typename T>
        json_value(const T *);
        template <typename T>
        json_value &operator=(T *);
        template <typename T>
        json_value &operator=(const T *);
    };

    VOGL_DEFINE_BITWISE_MOVABLE(json_value);

    inline json_value &get_null_json_value()
    {
        static json_value s_null_json_value;
        return s_null_json_value;
    }

    typedef vogl::vector<dynamic_string> dynamic_string_array;
    typedef vogl::vector<json_value> json_value_array;

    // A json_node can be either a JSON object (key/values) or array (values only).
    // It contains one or two separate unsorted arrays: one for keys (JSON objects only), and another for values (JSON objects or arrays).
    // json_node's are multisets, not sets, so duplicate keys do not cause errors, although the find() helpers will only find the first key.
    // The order of the key/value arrays is preserved in this design, but find()'s in large objects/array can be slow.
    // json_value's may point to other child json_nodes, which are allocated and freed from the heap.
    // The keys are simple zero terminated strings, and the values are instances of the json_value class above.
    // This design allows all keys/values at each object/array level to be stored in simple contiguous arrays.
    // Each json_node has a pointer to its parent, or NULL for the root.
    // Note that json_values which are not json_node's don't have pointers to their owners nodes, which can be annoying.
    class json_node
    {
        friend class json_value;

    public:
        json_node();
        json_node(const json_node &other);
        json_node(const json_node *pParent, bool is_object);
        ~json_node();

        json_node &operator=(const json_node &rhs);

        // Serialize to buffer.
        void serialize(vogl::vector<char> &buf, bool formatted = true, uint32_t cur_index = 0, bool null_terminate = true, uint32_t max_line_len = CMaxLineLenDefault) const;

        void serialize(dynamic_string &str, bool formatted = true, uint32_t cur_index = 0, uint32_t max_line_len = CMaxLineLenDefault) const;

        void binary_serialize(vogl::vector<uint8_t> &buf) const;

        // Parent/child retrieval
        inline const json_node *get_parent() const;

        // true if the value at the specified index is an object or array.
        inline bool is_child(uint32_t index) const;

        // true if the value at the specified index is an object.
        inline bool is_child_object(uint32_t index) const;

        // true if the value at the specified index is an array.
        inline bool is_child_array(uint32_t index) const;

        // Finds the specified child (case insensitive), or NULL if the child cannot be found.
        const json_node *find_child(const char *pKey) const;
        json_node *find_child(const char *pKey);

        // Finds the specified child object (case insensitive), or NULL if the child cannot be found or if the child is not an object.
        const json_node *find_child_object(const char *pKey) const;
        json_node *find_child_object(const char *pKey);

        // Finds the specified child object (case insensitive), or NULL if the child cannot be found or if the child is not an array.
        const json_node *find_child_array(const char *pKey) const;
        json_node *find_child_array(const char *pKey);

        // true if all children are plain values (i.e. not arrays or objects)
        bool are_all_children_values() const;

        // true if all children are objects
        bool are_all_children_objects() const;

        // true if all children are arrays
        bool are_all_children_arrays() const;

        // true if all children are objects or arrays
        bool are_all_children_objects_or_arrays() const;

        // Returns pointer to the child array or object at the specified index, or NULL if the value is not an array or object.
        inline const json_node *get_child(uint32_t index) const;
        inline json_node *get_child(uint32_t index);

        // true if any of the values in this node are objects or arrays.
        bool has_children() const;

        // Object/array info
        inline bool is_object() const;
        inline bool is_array() const;
        inline uint32_t size() const;

        // Key retrieval/finding

        // Returns cInvalidIndex (-1) if key was not found. Search is case insensitive.
        int find_key(const char *pKey) const;

        int find_child(const json_node *pNode) const;

        inline bool has_key(const char *pKey) const;

        // true if the value associated with the specified key is an object
        inline bool has_object(const char *pKey) const;

        // true if the value associated with the specified key is an array
        inline bool has_array(const char *pKey) const;

        inline const dynamic_string &get_key(uint32_t index) const;

        // returns get_null_json_value if the key does not exist
        const json_value &find_value(const char *pKey) const;
        inline const json_value &operator[](const char *pKey) const;

        json_value_type_t find_value_type(const char *pKey) const;

        // Value retrieval/finding
        inline json_value_type_t get_value_type(uint32_t index) const;
        inline const json_value &get_value(uint32_t index) const;
        inline const json_value &operator[](uint32_t index) const;

        // Returns NULL if the value is not an object or array.
        inline const json_node *get_value_as_object_or_array(uint32_t index) const;
        inline const json_node *get_value_as_object(uint32_t index) const;
        inline const json_node *get_value_as_array(uint32_t index) const;

        // get() variants returns true if key was found and the JSON value could be losslessly converted to the destination type.
        bool get_value_as_string(const char *pKey, dynamic_string &val, const char *pDef = "") const;
        bool get_value_as_enum(const char *pKey, const char **pStringList, int &val, int def = 0) const;

        // T may be int8_t, int16_t, int32_t or uint8_t, uint16_t, uint32_t
        // Internally this fetches the value as int64_t and attempts to convert. Returns false and sets val to def if out of range.
        template <typename T>
        bool get_value_as_int(const char *pKey, T &val, T def = 0) const;

        bool get_value_as_int64(const char *pKey, int64_t &val, int64_t def = 0) const;
        bool get_value_as_uint32(const char *pKey, uint32_t &val, uint32_t def = 0) const;
        bool get_value_as_uint64(const char *pKey, uint64_t &val, uint64_t def = 0) const;
        bool get_value_as_float(const char *pKey, float &val, float def = 0) const;
        bool get_value_as_double(const char *pKey, double &val, double def = 0) const;
        bool get_value_as_bool(const char *pKey, bool &val, bool def = false) const;

        // The value_as() variants find the first key matching pKey and return its value, or the default if key was not found.
        // Note that if trying to convert a negative value to an unsigned type, or a value which is too large will fail and you'll get the default.
        dynamic_string value_as_string(const char *pKey, const char *pDef = "") const;
        const char *value_as_string_ptr(const char *pKey, const char *pDef = "") const;
        int value_as_int(const char *pKey, int def = 0) const;
        int32_t value_as_int32(const char *pKey, int32_t def = 0) const;
        int64_t value_as_int64(const char *pKey, int64_t def = 0) const;
        uint32_t value_as_uint32(const char *pKey, uint32_t def = 0) const;
        uint64_t value_as_uint64(const char *pKey, uint64_t def = 0) const;
        float value_as_float(const char *pKey, float def = 0) const;
        double value_as_double(const char *pKey, double def = 0) const;
        bool value_as_bool(const char *pKey, bool def = false) const;

        // The value_as() variants return the value, or the default if key can't be converted to the desired type.
        // Note that if trying to convert a negative value to an unsigned type, or a value which is too large will fail and you'll get the default.
        dynamic_string value_as_string(uint32_t index, const char *pDef = "") const;
        // Returns NULL if value is not a string.
        const char *value_as_string_ptr(uint32_t index, const char *pDef = "") const;
        int value_as_int(uint32_t index, int def = 0) const;
        int32_t value_as_int32(uint32_t index, int32_t def = 0) const;
        int64_t value_as_int64(uint32_t index, int64_t def = 0) const;
        uint32_t value_as_uint32(uint32_t index, uint32_t def = 0) const;
        uint64_t value_as_uint64(uint32_t index, uint64_t def = 0) const;
        float value_as_float(uint32_t index, float def = 0) const;
        double value_as_double(uint32_t index, double def = 0) const;
        bool value_as_bool(uint32_t index, bool def = false) const;

        inline bool operator==(const json_node &other) const;
        inline bool operator!=(const json_node &other) const;

        // Modification
        // TODO: Add splicing, insert

        // Retrieves the json_value at the specified index.
        inline json_value &get_value(uint32_t index);
        inline json_value &operator[](uint32_t index);

        // Sets the parent node of this node.
        void set_parent(const json_node *pParent);

        // Completely clears the node.
        void clear();

        // Sets this node to an object. If the node is an array it will be upgraded to an object with an array of empty keys.
        void set_is_object(bool is_object);
        void init_object();
        void init_array();

        // Changes the number of elements in this node. Enlarging will add empty keys (for objects), and values of type cJSONValueTypeNull.
        void resize(uint32_t new_size);
        inline uint32_t enlarge(uint32_t n);

        void reserve(uint32_t new_capacity);

        // Retrieves the json_value associated with a key, or adds a new named key/value to the node, which must be an object. The newly added json_value will have type cJSONValueTypeNull.
        json_value &get_or_add(const char *pKey);
        json_value &get_or_add(const dynamic_string &key);

        // Adds a named key/value to the node. The json_value will have type cJSONValueTypeNull. If the node is not an object it will be upgraded to an object.
        json_value &add(const char *pKey);
        json_value &add(const dynamic_string &key);

        // Adds a named child object to the node. If the node is not an object it will be upgraded to an object.
        json_node &add_object(const char *pKey);
        json_node &add_object(const dynamic_string &key);

        // Adds an unnamed object to the node.
        json_node &add_object();

        // Adds a named child array to the node. If the node is not an object it will be upgraded to an object.
        json_node &add_array(const char *pKey);
        json_node &add_array(const dynamic_string &key);

        // Adds an unnamed array to the node.
        json_node &add_array();

        // Adds a key/value to a node. If the node is not an object it will be upgraded to an object.
        json_value &add_key_value(const char *pKey, const json_value &val);
        json_value &add_key_value(const dynamic_string &key, const json_value &val);

        // Adds a new value to a node. If the node is an object then an empty key will be associated with the newly added value.
        json_value &add_value();
        json_value &add_value(const json_value &val);

        // Adds a new key/value to a node. pValueToParse must be JSON text.
        bool add_key_and_parsed_value(const char *pKey, const char *pValueToParse);

        // Adds a new value to a node. pValueToParse must be JSON text.
        bool add_parsed_value(const char *pValueToParse);

        // Retrieves a node's key. Asserts and returns a ref to get_empty_dynamic_string() if the node is an array.
        dynamic_string &get_key(uint32_t index);

        // Sets the key/value at the specified index. If the node is not an object it will be upgraded to one.
        void set_key_value(uint32_t index, const char *pKey, const json_value &val);
        void set_key(uint32_t index, const char *pKey);
        void set_value(uint32_t index, const json_value &val);

        // Sets the value at the specified index. Takes ownership of any node pointed to by val. val will be cleared.
        void set_value_assume_ownership(uint32_t index, json_value &val);
        void add_value_assume_ownership(json_value &val);
        void add_key_value_assume_ownership(const char *pKey, json_value &val);

        // Removes the specified key (case insensitive), returns true if the key is found.
        bool erase(const char *pKey);
        bool erase(const dynamic_string &key);

        // Removes the specified key at index.
        void erase(uint32_t index);

        // true if the node passes some basic tests for validity.
        bool basic_validation(const json_node *pParent) const;

        // true if the node, and its children, pass some basic tests for validity.
        bool validate(const json_node *pParent) const;

        // Returns a path string to this node, i.e. [root]/blah/[5]/cool, where cool is the current node, [5] is cool's parent (array item 5), etc.
        dynamic_string get_path_to_node() const;

        // pKey is not validated.
        dynamic_string get_path_to_key(const char *pKey) const;

        // index is not range checked.
        dynamic_string get_path_to_item(uint32_t index) const;

        // Returns true if one or more keys has the same name, which is not valid JSON to many readers (although this seems to be a grey area of the spec).
        // Recursively checks children nodes.
        bool check_for_duplicate_keys() const;

        // Set the line number associated with this node.
        void set_line(uint32_t line);

        // Gets the line number associated with this node.
        uint32_t get_line() const;

        // High-level container serialization/deserialization to/from JSON nodes.
        // These methods add or retrieve entire containers (vectors, growable_array's, hash_map's, or map's) or user objects to/from JSON nodes.
        // They assume the object, element or key/value types have overloaded "json_serialize" and "json_deserialize" functions defined, or you've defined overloaded json_serialize/json_deserialize members in the voglcore namespace.

        // Adds a named or unnamed object to this node.
        template <typename T>
        bool add_object(const char *pKey, const T &obj);

        // Retrieves a named object from this node.
        template <typename T>
        bool get_object(const char *pKey, T &obj) const;

        // Retrieves the object at the specified index from this node.
        template <typename T>
        bool get_object(uint32_t index, T &obj) const;

        // add_vector() and get_vector() are compatible with vogl::vector or vogl::growable_array.
        // If pKey is NULL, the hash map is read from the current node, otherwise it's read from the child object named pKey.
        template <typename T>
        bool add_vector(const char *pKey, const T &vec);

        // If pKey is NULL, the hash mode is read from the current node, otherwise it's read from a child named pKey.
        template <typename T>
        bool get_vector(const char *pKey, T &vec) const;

        // add_map() and get_map() are compatible with vogl::map and vogl::hash_map.
        // If pKey is NULL, the hash mode is added to the current node, otherwise it's added to a child named pKey.
        template <typename T>
        bool add_map(const char *pKey, const T &hash_map);

        // If pKey is NULL, the hash map is read from the current node, otherwise it's read from the child object named pKey.
        template <typename T>
        bool get_map(const char *pKey, T &hash_map) const;

    private:
        const json_node *m_pParent;

        dynamic_string_array m_keys;
        json_value_array m_values;

        uint32_t m_line;

        bool m_is_object;

        void ensure_is_object();
        void serialize(json_growable_char_buf &buf, bool formatted, uint32_t cur_index, uint32_t max_line_len = CMaxLineLenDefault) const;
    };

    // json_document is an optional helper class that derives from json_value.
    // It may optionally have been parsed from a file, string, or buffer and contains filename/error/line information.
    // It's not derived from json_node because a valid JSON document may be a single value and not necessarily an object or array, although in practice it will be an object or maybe an array (and NOT a plain value) 99.999% of the time.
    class json_document : public json_value
    {
        typedef json_value base;

    public:
        json_document();
        explicit json_document(const json_value &other);
        json_document(const json_document &other);
        json_document(const char *pStr, const char *pFilename = "<string>");
        json_document(const char *pBuf, uint32_t n, const char *pFilename = "<buffer>");
        json_document(json_value_type_t value_type);
        ~json_document();

        json_document &operator=(const json_document &rhs);
        json_document &operator=(const json_value &rhs);

        // root value manipulation
        inline const json_value &get_value() const;
        inline json_value &get_value();

        // This returns a pointer because a valid JSON document can be a single value (in which case the document's value is not a node pointer).
        inline const json_node *get_root() const;
        inline json_node *get_root();

        inline void clear(bool reinitialize_to_object = true);

        // Swaps two documents.
        void swap(json_document &other);

        // Deserialize UTF8 text from files, buffer, or a string.
        bool deserialize_file(const char *pFilename);
        bool deserialize(FILE *pFile, const char *pFilename = "<FILE>");
        bool deserialize(const vogl::vector<char> &buf, const char *pFilename = "<buffer>");
        bool deserialize(const char *pBuf, size_t n, const char *pFilename = "<buffer>");
        bool deserialize(const char *pStr, const char *pFilename = "<string>");
        bool deserialize(const dynamic_string &str, const char *pFilename = "<string>");

        // document's filename
        inline const dynamic_string &get_filename() const;
        inline void set_filename(const char *pFilename);

        // error messages due to failed parses
        const dynamic_string &get_error_msg() const;
        uint32_t get_error_line() const;
        void clear_error();

    private:
        dynamic_string m_filename;

        dynamic_string m_error_msg;
        uint32_t m_error_line;
    };

    bool json_test();

} // namespace vogl

#include "vogl_json.inl"
