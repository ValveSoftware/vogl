// File: vogl_json.inl

namespace vogl
{

    inline json_value::json_value()
        : m_type(cJSONValueTypeNull), m_line(0)
    {
        m_data.m_nVal = 0;
    }

    inline json_value::json_value(bool val)
        : m_type(cJSONValueTypeBool), m_line(0)
    {
        m_data.m_nVal = val;
    }

    inline json_value::json_value(int32_t nVal)
        : m_type(cJSONValueTypeInt), m_line(0)
    {
        m_data.m_nVal = nVal;
    }

    inline json_value::json_value(uint32_t nVal)
        : m_type(cJSONValueTypeInt), m_line(0)
    {
        m_data.m_nVal = nVal;
    }

    inline json_value::json_value(int64_t nVal)
        : m_type(cJSONValueTypeInt), m_line(0)
    {
        m_data.m_nVal = nVal;
    }

    // Note uint64_t values may be encoded as hex strings or int64_t
    inline json_value::json_value(uint64_t nVal)
        : m_type(cJSONValueTypeNull), m_line(0)
    {
        m_data.m_nVal = 0;
        set_value(nVal);
    }

    inline json_value::json_value(double flVal)
        : m_type(cJSONValueTypeDouble), m_line(0)
    {
        m_data.m_flVal = flVal;
    }

    inline json_value::json_value(char *pStr)
        : m_type(cJSONValueTypeString), m_line(0)
    {
        m_data.m_pStr = vogl_strdup(pStr);
    }

    inline json_value::json_value(const char *pStr)
        : m_type(cJSONValueTypeString), m_line(0)
    {
        m_data.m_pStr = vogl_strdup(pStr);
    }

    inline json_value::json_value(const dynamic_string &str)
        : m_type(cJSONValueTypeString), m_line(0)
    {
        m_data.m_pStr = vogl_strdup(str.get_ptr());
    }

    inline json_value::json_value(const json_node *pNode)
        : m_type(cJSONValueTypeNode), m_line(0)
    {
        m_data.m_pNode = get_json_node_pool()->alloc(*pNode);
    }

    inline json_value::json_value(json_value_type_t type)
        : m_type(type),
          m_line(0)
    {
        m_data.m_nVal = 0;
        if (type == cJSONValueTypeNode)
            m_data.m_pNode = get_json_node_pool()->alloc();
        else if (type == cJSONValueTypeString)
            m_data.m_pStr = vogl_strdup("");
    }

    inline json_value::~json_value()
    {
        free_data();
    }

    inline json_value_type_t json_value::get_type() const
    {
        return m_type;
    }

    inline const json_value_data_t &json_value::get_raw_data() const
    {
        return m_data;
    }

    inline json_value_data_t &json_value::get_raw_data()
    {
        return m_data;
    }

    inline bool json_value::is_null() const
    {
        return m_type == cJSONValueTypeNull;
    }

    inline bool json_value::is_valid() const
    {
        return m_type != cJSONValueTypeNull;
    }

    inline bool json_value::is_bool() const
    {
        return m_type == cJSONValueTypeBool;
    }

    inline bool json_value::is_int() const
    {
        return m_type == cJSONValueTypeInt;
    }

    inline bool json_value::is_double() const
    {
        return m_type == cJSONValueTypeDouble;
    }

    inline bool json_value::is_numeric() const
    {
        return (m_type == cJSONValueTypeInt) || (m_type == cJSONValueTypeDouble);
    }

    inline bool json_value::is_string() const
    {
        return m_type == cJSONValueTypeString;
    }

    inline bool json_value::is_node() const
    {
        return m_type == cJSONValueTypeNode;
    }

    inline bool json_value::is_object_or_array() const
    {
        return m_type == cJSONValueTypeNode;
    }

    inline const json_node *json_value::get_node_ptr() const
    {
        return (m_type == cJSONValueTypeNode) ? m_data.m_pNode : NULL;
    }
    inline json_node *json_value::get_node_ptr()
    {
        return (m_type == cJSONValueTypeNode) ? m_data.m_pNode : NULL;
    }

    inline void json_value::clear()
    {
        set_value_to_null();
        m_line = 0;
    }

    inline void json_value::assume_ownership(json_value &src_val)
    {
        set_value_to_null();
        swap(src_val);
    }

    inline void json_value::release_ownership(json_value &dst_value)
    {
        dst_value.set_value_to_null();
        dst_value.swap(*this);
    }

    inline void json_value::init(json_value_type_t type)
    {
        free_data();
        m_type = type;
        m_data.m_nVal = 0;
        if (type == cJSONValueTypeNode)
            m_data.m_pNode = get_json_node_pool()->alloc();
        else if (type == cJSONValueTypeString)
            m_data.m_pStr = vogl_strdup("");
        m_line = 0;
    }

    inline json_node *json_value::init_object()
    {
        return set_value_to_node(true);
    }

    inline json_node *json_value::init_array()
    {
        return set_value_to_node(false);
    }

    inline json_node *json_value::set_value_to_object(json_node *pParent)
    {
        json_node *pRoot = get_json_node_pool()->alloc(pParent, true);
        set_value_assume_ownership(pRoot);
        return pRoot;
    }

    inline json_node *json_value::set_value_to_array(json_node *pParent)
    {
        json_node *pRoot = get_json_node_pool()->alloc(pParent, false);
        set_value_assume_ownership(pRoot);
        return pRoot;
    }

    inline json_node *json_value::set_value_to_node(bool is_object, json_node *pParent)
    {
        json_node *pRoot = get_json_node_pool()->alloc(pParent, is_object);
        set_value_assume_ownership(pRoot);
        return pRoot;
    }

    inline void json_value::set_value_to_null()
    {
        free_data();
        m_data.m_nVal = 0;
        m_type = cJSONValueTypeNull;
    }

    inline void json_value::set_value(bool val)
    {
        free_data();
        m_data.m_nVal = val;
        m_type = cJSONValueTypeBool;
    }

    inline void json_value::set_value(int8_t nVal)
    {
        free_data();
        m_data.m_nVal = nVal;
        m_type = cJSONValueTypeInt;
    }

    inline void json_value::set_value(int16_t nVal)
    {
        free_data();
        m_data.m_nVal = nVal;
        m_type = cJSONValueTypeInt;
    }

    inline void json_value::set_value(int32_t nVal)
    {
        free_data();
        m_data.m_nVal = nVal;
        m_type = cJSONValueTypeInt;
    }

    inline void json_value::set_value(int64_t nVal)
    {
        free_data();
        m_data.m_nVal = nVal;
        m_type = cJSONValueTypeInt;
    }

    inline void json_value::set_value(uint8_t nVal)
    {
        set_value(static_cast<int64_t>(nVal));
    }

    inline void json_value::set_value(uint16_t nVal)
    {
        set_value(static_cast<int64_t>(nVal));
    }

    inline void json_value::set_value(uint32_t nVal)
    {
        set_value(static_cast<int64_t>(nVal));
    }

    inline void json_value::set_value(double flVal)
    {
        free_data();
        m_data.m_flVal = flVal;
        m_type = cJSONValueTypeDouble;
    }

    inline void json_value::set_value(const char *pStr)
    {
        free_data();
        m_data.m_pStr = vogl_strdup(pStr);
        m_type = cJSONValueTypeString;
    }

    inline void json_value::set_value(const dynamic_string &str)
    {
        free_data();
        m_data.m_pStr = vogl_strdup(str.get_ptr());
        m_type = cJSONValueTypeString;
    }

    inline void json_value::set_value(const json_node *pNode)
    {
        free_data();
        m_data.m_pNode = get_json_node_pool()->alloc(*pNode);
        m_type = cJSONValueTypeNode;
    }

    inline void json_value::set_value_assume_ownership(char *pStr)
    {
        free_data();
        m_data.m_pStr = pStr;
        m_type = cJSONValueTypeString;
    }

    inline void json_value::set_value_assume_ownership(json_node *pNode)
    {
        free_data();
        m_data.m_pNode = pNode;
        m_type = cJSONValueTypeNode;
    }

    inline json_value &json_value::operator=(bool val)
    {
        set_value(val);
        return *this;
    }

    inline json_value &json_value::operator=(int32_t nVal)
    {
        set_value(nVal);
        return *this;
    }

    inline json_value &json_value::operator=(uint32_t nVal)
    {
        set_value(nVal);
        return *this;
    }

    inline json_value &json_value::operator=(int64_t nVal)
    {
        set_value(nVal);
        return *this;
    }

    inline json_value &json_value::operator=(uint64_t nVal)
    {
        set_value(nVal);
        return *this;
    }

    inline json_value &json_value::operator=(double flVal)
    {
        set_value(flVal);
        return *this;
    }

    inline json_value &json_value::operator=(const char *pStr)
    {
        set_value(pStr);
        return *this;
    }

    inline bool json_value::is_object() const
    {
        return (m_type == cJSONValueTypeNode) ? get_node_ptr()->is_object() : false;
    }

    inline bool json_value::is_array() const
    {
        return (m_type == cJSONValueTypeNode) ? get_node_ptr()->is_array() : false;
    }

    inline bool json_value::operator==(const json_value &other) const
    {
        if (m_type != other.m_type)
            return false;

        switch (m_type)
        {
            case cJSONValueTypeNull:
                return true;
            case cJSONValueTypeBool:
            case cJSONValueTypeInt:
            case cJSONValueTypeDouble:
                return m_data.m_nVal == other.m_data.m_nVal;
            case cJSONValueTypeString:
                return vogl_strcmp(as_string_ptr(), other.as_string_ptr()) == 0;
            case cJSONValueTypeNode:
                return *get_node_ptr() == *other.get_node_ptr();
        }
        return false;
    }

    // Attempts to convert a JSON value to each type.
    // Returns false and sets val to def if the value cannot be converted (out of range, or type is obviously incompatible).
    // Note that if trying to convert a negative value to an unsigned type, or a value which is too large will fail and you'll get the default.
    inline bool json_value::get_bool(bool &val, bool def) const
    {
        if (is_bool())
        {
            val = (m_data.m_nVal != 0);
            return true;
        }
        else
            return convert_to_bool(val, def);
    }

    inline bool json_value::get_numeric(int32_t &val, int32_t def) const
    {
        if ((is_int()) && (m_data.m_nVal == static_cast<int32_t>(m_data.m_nVal)))
        {
            val = static_cast<int32_t>(m_data.m_nVal);
            return true;
        }
        else
            return convert_to_int32(val, def);
    }

    inline bool json_value::get_numeric(int64_t &val, int64_t def) const
    {
        if (is_int())
        {
            val = m_data.m_nVal;
            return true;
        }
        else
            return convert_to_int64(val, def);
    }

    inline bool json_value::get_numeric(uint64_t &val, uint64_t def) const
    {
        return convert_to_uint64(val, def);
    }

    inline bool json_value::get_numeric(float &val, float def) const
    {
        if (is_double())
        {
            val = static_cast<float>(m_data.m_flVal);
            return true;
        }
        else
            return convert_to_float(val, def);
    }

    inline bool json_value::get_numeric(double &val, double def) const
    {
        if (is_double())
        {
            val = m_data.m_flVal;
            return true;
        }
        else
            return convert_to_double(val, def);
    }

    inline bool json_value::get_string(dynamic_string &val, const char *pDef) const
    {
        if (is_string())
        {
            val = m_data.m_pStr;
            return true;
        }
        else
            return convert_to_string(val, pDef);
    }

    inline bool json_value::as_bool(bool def) const
    {
        bool result;
        get_bool(result, def);
        return result;
    }

    inline int json_value::as_int(int def) const
    {
        int32_t result;
        get_numeric(result, def);
        return result;
    }

    inline int32_t json_value::as_int32(int32_t def) const
    {
        int32_t result;
        get_numeric(result, def);
        return result;
    }

    inline uint32_t json_value::as_uint32(uint32_t def) const
    {
        uint32_t result;
        get_numeric(result, def);
        return result;
    }

    inline int64_t json_value::as_int64(int64_t def) const
    {
        int64_t result;
        get_numeric(result, def);
        return result;
    }

    inline uint64_t json_value::as_uint64(uint64_t def) const
    {
        uint64_t result;
        get_numeric(result, def);
        return result;
    }

    inline float json_value::as_float(float def) const
    {
        float result;
        get_numeric(result, def);
        return result;
    }

    inline double json_value::as_double(double def) const
    {
        double result;
        get_numeric(result, def);
        return result;
    }

    // Returns value as a string, or the default string if the value cannot be converted.
    inline dynamic_string json_value::as_string(const char *pDef) const
    {
        dynamic_string result;
        get_string(result, pDef);
        return result;
    }

    // Returns pointer to null terminated string or NULL if the value is not a string.
    inline const char *json_value::as_string_ptr(const char *pDef) const
    {
        return is_string() ? m_data.m_pStr : pDef;
    }

    inline bool json_value::operator!=(const json_value &other) const
    {
        return !(*this == other);
    }

    inline void json_value::swap(json_value &other)
    {
        std::swap(m_type, other.m_type);
        VOGL_ASSUME(sizeof(m_data.m_nVal) == sizeof(m_data));
        std::swap(m_data.m_nVal, other.m_data.m_nVal);
    }

    inline void json_value::set_line(uint32_t line)
    {
        m_line = line;
    }

    inline uint32_t json_value::get_line() const
    {
        return m_line;
    }

    template <typename T>
    bool json_node::get_value_as_int(const char *pKey, T &val, T def) const
    {
        val = def;

        int index = find_key(pKey);
        if (index < 0)
            return false;

        int64_t v;
        if (!get_value(index).get_numeric(v, 0))
            return false;

        if ((v < int_traits<T>::cMin) || (v > int_traits<T>::cMax))
            return false;
        val = static_cast<T>(v);
        return true;
    }

    inline bool json_node::operator==(const json_node &other) const
    {
        if (m_is_object != other.m_is_object)
            return false;
        if (m_keys.size() != other.m_keys.size())
            return false;
        if (m_values.size() != other.m_values.size())
            return false;
        if (m_is_object)
        {
            for (uint32_t i = 0; i < m_values.size(); i++)
            {
                int j = other.find_key(m_keys[i].get_ptr());
                if (j == cInvalidIndex)
                    return false;

                if (m_values[i] != other.m_values[j])
                    return false;
            }
        }
        else
        {
            for (uint32_t i = 0; i < m_values.size(); i++)
            {
                if (m_values[i] != other.m_values[i])
                    return false;
            }
        }
        return true;
    }

    // Parent/child retrieval
    inline const json_node *json_node::get_parent() const
    {
        return m_pParent;
    }

    // true if the value at the specified index is an object or array.
    inline bool json_node::is_child(uint32_t index) const
    {
        return get_value_type(index) == cJSONValueTypeNode;
    }

    // true if the value at the specified index is an object.
    inline bool json_node::is_child_object(uint32_t index) const
    {
        const json_node *pChild = get_child(index);
        if (!pChild)
            return false;
        return pChild->is_object();
    }

    // true if the value at the specified index is an array.
    inline bool json_node::is_child_array(uint32_t index) const
    {
        const json_node *pChild = get_child(index);
        if (!pChild)
            return false;
        return pChild->is_array();
    }

    // Returns pointer to the child array or object at the specified index, or NULL if the value is not an array or object.
    inline const json_node *json_node::get_child(uint32_t index) const
    {
        return m_values[index].get_node_ptr();
    }

    inline json_node *json_node::get_child(uint32_t index)
    {
        return m_values[index].get_node_ptr();
    }

    // Object/array info
    inline bool json_node::is_object() const
    {
        return m_is_object;
    }

    inline bool json_node::is_array() const
    {
        return !m_is_object;
    }

    inline uint32_t json_node::size() const
    {
        return m_values.size();
    }

    inline bool json_node::has_key(const char *pKey) const
    {
        return find_key(pKey) >= 0;
    }

    // true if the value associated with the specified key is an object
    inline bool json_node::has_object(const char *pKey) const
    {
        int index = find_key(pKey);
        if (index < 0)
            return false;
        return is_child_object(index);
    }

    // true if the value associated with the specified key is an array
    inline bool json_node::has_array(const char *pKey) const
    {
        int index = find_key(pKey);
        if (index < 0)
            return false;
        return is_child_array(index);
    }

    inline const dynamic_string &json_node::get_key(uint32_t index) const
    {
        return m_keys[index];
    }

    // returns get_null_json_value() if the key does not exist
    inline const json_value &json_node::operator[](const char *pKey) const
    {
        return find_value(pKey);
    }

    // Value retrieval/finding
    inline json_value_type_t json_node::get_value_type(uint32_t index) const
    {
        return m_values[index].get_type();
    }

    inline const json_value &json_node::get_value(uint32_t index) const
    {
        return m_values[index];
    }

    inline const json_value &json_node::operator[](uint32_t index) const
    {
        return m_values[index];
    }

    // Returns NULL if the value is not an object or array.
    inline const json_node *json_node::get_value_as_object_or_array(uint32_t index) const
    {
        return get_child(index);
    }

    inline const json_node *json_node::get_value_as_object(uint32_t index) const
    {
        if (!is_child_object(index))
            return NULL;
        else
            return get_child(index);
    }

    inline const json_node *json_node::get_value_as_array(uint32_t index) const
    {
        if (!is_child_array(index))
            return NULL;
        else
            return get_child(index);
    }

    inline bool json_node::operator!=(const json_node &other) const
    {
        return !(*this == other);
    }

    // Retrieves the json_value at the specified index.
    inline json_value &json_node::get_value(uint32_t index)
    {
        return m_values[index];
    }

    inline json_value &json_node::operator[](uint32_t index)
    {
        return m_values[index];
    }

    // Sets the parent node of this node.
    inline void json_node::set_parent(const json_node *pParent)
    {
        m_pParent = pParent;
    }

    inline void json_node::init_object()
    {
        set_is_object(true);
    }

    inline void json_node::init_array()
    {
        set_is_object(false);
    }

    // Changes the number of elements in this node. Enlarging will add empty keys (for objects), and values of type cJSONValueTypeNull.
    inline uint32_t json_node::enlarge(uint32_t n)
    {
        uint32_t cur_size = size();
        resize(cur_size + n);
        return cur_size;
    }

    // Retrieves the json_value associated with a key, or adds a new named key/value to the node, which must be an object. The newly added json_value will have type cJSONValueTypeNull.
    inline json_value &json_node::get_or_add(const dynamic_string &key)
    {
        return get_or_add(key.get_ptr());
    }

    // Adds a named key/value to the node. The json_value will have type cJSONValueTypeNull. If the node is not an object it will be upgraded to an object.
    inline json_value &json_node::add(const dynamic_string &key)
    {
        return add(key.get_ptr());
    }

    // Adds a named child object to the node. If the node is not an object it will be upgraded to an object.
    inline json_node &json_node::add_object(const dynamic_string &key)
    {
        return add_object(key.get_ptr());
    }

    // Adds a named child array to the node. If the node is not an object it will be upgraded to an object.
    inline json_node &json_node::add_array(const dynamic_string &key)
    {
        return add_array(key.get_ptr());
    }

    // Adds a key/value to a node. If the node is not an object it will be upgraded to an object.
    inline json_value &json_node::add_key_value(const dynamic_string &key, const json_value &val)
    {
        return add_key_value(key.get_ptr(), val);
    }

    inline bool json_node::erase(const dynamic_string &key)
    {
        return erase(key.get_ptr());
    }

    // Set the light number associated with this node.
    inline void json_node::set_line(uint32_t line)
    {
        m_line = line;
    }

    // Gets the light number associated with this node.
    inline uint32_t json_node::get_line() const
    {
        return m_line;
    }

    template <typename T>
    bool json_node::add_object(const char *pKey, const T &obj)
    {
        json_value &val = pKey ? add(pKey) : add_value();
        return json_serialize(obj, val);
    }

    template <typename T>
    bool json_node::get_object(const char *pKey, T &obj) const
    {
        if (!pKey)
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        const json_value &val = find_value(pKey);
        if (val == get_null_json_value())
            return false;

        return json_deserialize(obj, val);
    }

    template <typename T>
    bool json_node::get_object(uint32_t index, T &obj) const
    {
        return json_deserialize(obj, get_value(index));
    }

    template <typename T>
    inline bool json_node::add_vector(const char *pKey, const T &vec)
    {
        json_node &obj = pKey ? add_object(pKey) : *this;

        obj.add_key_value("type", "vector");

        uint32_t size = vec.size();
        obj.add_key_value("size", size);

        if (size)
        {
            json_node &elements = obj.add_array("elements");
            elements.reserve(size);

            for (uint32_t i = 0; i < size; i++)
            {
                json_value &new_val = elements.add_value();
                if (!json_serialize(vec[i], new_val))
                    return false;

                if (new_val.is_node())
                    new_val.get_node_ptr()->set_parent(&elements);
            }
        }

        return true;
    }

    template <typename T>
    inline bool json_node::get_vector(const char *pKey, T &vec) const
    {
        const json_node *pObj = pKey ? find_child_object(pKey) : this;
        if (!pObj)
            return false;

        if (!pObj->is_object())
            return false;

        if (pObj->value_as_string("type") != "vector")
            return false;

        uint64_t size64 = pObj->value_as_uint64("size");
        if (size64 > cUINT32_MAX)
            return false;

        uint32_t size = static_cast<uint32_t>(size64);

        vec.resize(size);

        if (size)
        {
            const json_node *pElements = pObj->find_child_array("elements");

            if ((!pElements) || (pElements->size() != size))
                return false;

            for (uint32_t i = 0; i < size; i++)
                if (!json_deserialize(vec[i], pElements->get_value(i)))
                    return false;
        }

        return true;
    }

    template <typename T>
    inline bool json_node::add_map(const char *pKey, const T &map)
    {
        json_node &obj = pKey ? add_object(pKey) : *this;

        obj.add_key_value("type", "map");
        obj.add_key_value("size", map.size());

        if (map.size())
        {
            json_node &arr_node = obj.add_array("objects");
            arr_node.reserve(map.size());

            typename T::const_iterator end_it(map.end());
            for (typename T::const_iterator it = map.begin(); it != end_it; ++it)
            {
                json_node &element = arr_node.add_object();

                json_value &val = element.add("key");
                json_serialize(it->first, val);
                if (val.is_node())
                    val.get_node_ptr()->set_parent(&arr_node);

                if (!is_empty_type<typename T::referent_type>::cValue)
                {
                    json_value &val2 = element.add("value");
                    json_serialize(it->second, val2);
                    if (val2.is_node())
                        val2.get_node_ptr()->set_parent(&arr_node);
                }
            }
        }

        return true;
    }

    template <typename T>
    inline bool json_node::get_map(const char *pKey, T &map) const
    {
        const json_node *pObj = pKey ? find_child_object(pKey) : this;
        if (!pObj)
            return false;

        if (!pObj->is_object())
            return false;

        if (pObj->value_as_string("type") != "map")
            return false;

        if (!pObj->has_key("size"))
            return false;

        uint64_t size64 = pObj->value_as_uint32("size");
        if ((size64) && (!pObj->has_key("objects")))
            return false;

        // TODO: fix this once vogl::vector supports 64-bit arrays
        if (size64 > cUINT32_MAX)
            return false;

        map.reset();

        if (!size64)
            return true;

        uint32_t size = static_cast<uint32_t>(size64);

        map.reserve(size);

        const json_node *pObjects_arr = pObj->find_child_array("objects");
        if (!pObjects_arr)
            return false;

        if (pObjects_arr->size() != size)
            return false;

        for (uint32_t i = 0; i < pObjects_arr->size(); i++)
        {
            const json_node *pArr_node = pObjects_arr->get_child(i);
            if (!pArr_node)
                return false;

            const json_value &key_val = pArr_node->find_value("key");
            if (key_val == get_null_json_value())
                return false;

            typename T::key_type key;
            if (!json_deserialize(key, key_val))
                return false;

            typename T::referent_type val;

            if (!is_empty_type<typename T::referent_type>::cValue)
            {
                const json_value &value_val = pArr_node->find_value("value");
                if (value_val == get_null_json_value())
                    return false;

                if (!json_deserialize(val, value_val))
                    return false;
            }

            if (!map.insert(key, val).second)
                return false;
        }

        return true;
    }

    template <typename T>
    inline bool json_serialize(const T &v, json_value &val)
    {
        return v.json_serialize(val);
    }

    template <typename T>
    inline bool json_deserialize(T &v, const json_value &val)
    {
        return v.json_deserialize(val);
    }

    inline bool json_serialize(const int8_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(int8_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const int16_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(int16_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const int32_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(int32_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const int64_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(int64_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const uint8_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(uint8_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const uint16_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(uint16_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const uint32_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(uint32_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const uint64_t &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(uint64_t &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const float &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(float &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const double &v, json_value &val)
    {
        val = v;
        return true;
    }

    inline bool json_deserialize(double &v, const json_value &val)
    {
        return val.get_numeric(v);
    }

    inline bool json_serialize(const json_node &v, json_value &val)
    {
        val.set_value(&v);
        return true;
    }

    inline bool json_deserialize(json_node &v, const json_value &val)
    {
        if (!val.is_object_or_array())
            return false;
        v = *val.get_node_ptr();
        return true;
    }

    template <typename Key, typename Value, typename Hasher, typename Equals>
    inline bool json_serialize(const hash_map<Key, Value, Hasher, Equals> &hash_map, json_value &val)
    {
        json_node *pObj = val.init_object();
        return pObj->add_map(NULL, hash_map);
    }

    template <typename Key, typename Value, typename Hasher, typename Equals>
    inline bool json_deserialize(hash_map<Key, Value, Hasher, Equals> &hash_map, const json_value &val)
    {
        if (!val.is_object())
            return false;
        const json_node *pNode = val.get_node_ptr();
        return pNode->get_map(NULL, hash_map);
    }

    template <typename Key, typename Value, typename Hasher, typename Equals, typename Allocator>
    inline bool json_serialize(const rh_hash_map<Key, Value, Hasher, Equals, Allocator> &rh_hash_map, json_value &val)
    {
        json_node *pObj = val.init_object();
        return pObj->add_map(NULL, rh_hash_map);
    }

    template <typename Key, typename Value, typename Hasher, typename Equals, typename Allocator>
    inline bool json_deserialize(rh_hash_map<Key, Value, Hasher, Equals, Allocator> &rh_hash_map, const json_value &val)
    {
        if (!val.is_object())
            return false;
        const json_node *pNode = val.get_node_ptr();
        return pNode->get_map(NULL, rh_hash_map);
    }

    template <typename Key, typename Value, typename LessComp, typename EqualComp, uint32_t MaxLevels>
    inline bool json_serialize(const vogl::map<Key, Value, LessComp, EqualComp, MaxLevels> &map, json_value &val)
    {
        json_node *pObj = val.init_object();
        return pObj->add_map(NULL, map);
    }

    template <typename Key, typename Value, typename LessComp, typename EqualComp, uint32_t MaxLevels>
    inline bool json_deserialize(vogl::map<Key, Value, LessComp, EqualComp, MaxLevels> &map, const json_value &val)
    {
        if (!val.is_object())
            return false;
        const json_node *pNode = val.get_node_ptr();
        return pNode->get_map(NULL, map);
    }

    template <typename T>
    inline bool json_serialize(const vogl::vector<T> &vec, json_value &val)
    {
        json_node *pObj = val.init_object();
        return pObj->add_vector(NULL, vec);
    }

    template <typename T>
    inline bool json_deserialize(vogl::vector<T> &vec, const json_value &val)
    {
        if (!val.is_object())
            return false;
        const json_node *pNode = val.get_node_ptr();
        return pNode->get_vector(NULL, vec);
    }

    template <typename T, uint32_t N>
    inline bool json_serialize(const vogl::growable_array<T, N> &vec, json_value &val)
    {
        json_node *pObj = val.init_object();
        return pObj->add_vector(NULL, vec);
    }

    template <typename T, uint32_t N>
    inline bool json_deserialize(vogl::growable_array<T, N> &vec, const json_value &val)
    {
        if (!val.is_object())
            return false;
        const json_node *pNode = val.get_node_ptr();
        return pNode->get_vector(NULL, vec);
    }

    // root value manipulation
    inline const json_value &json_document::get_value() const
    {
        return *this;
    }

    inline json_value &json_document::get_value()
    {
        return *this;
    }

    // This returns a pointer because a valid JSON document can be a single value (in which case the document's value is not a node pointer).
    inline const json_node *json_document::get_root() const
    {
        return get_node_ptr();
    }

    inline json_node *json_document::get_root()
    {
        return get_node_ptr();
    }

    inline void json_document::clear(bool reinitialize_to_object)
    {
        json_value::clear();
        if (reinitialize_to_object)
            init_object();
    }

    // document's filename
    inline const dynamic_string &json_document::get_filename() const
    {
        return m_filename;
    }

    inline void json_document::set_filename(const char *pFilename)
    {
        m_filename.set(pFilename ? pFilename : "");
    }

    // error messages due to failed parses
    inline const dynamic_string &json_document::get_error_msg() const
    {
        return m_error_msg;
    }

    inline uint32_t json_document::get_error_line() const
    {
        return m_error_line;
    }

    inline void json_document::clear_error()
    {
        m_error_msg.clear();
        m_error_line = 0;
    }

} // namespace vogl
