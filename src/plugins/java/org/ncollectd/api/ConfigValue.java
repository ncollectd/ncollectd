// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.api;

/**
 * Java representation of ncollectd/src/libconfig/config.h:config_value_t structure.
 *
 * @author Florian Forster &lt;octo at collectd.org&gt;
 */
public class ConfigValue
{
    public static final int CONFIG_TYPE_STRING  = 0;
    public static final int CONFIG_TYPE_NUMBER  = 1;
    public static final int CONFIG_TYPE_BOOLEAN = 2;

    private int         _type;
    private String  _value_string;
    private Number  _value_number;
    private boolean _value_boolean;

    public ConfigValue (String s)
    {
        _type = CONFIG_TYPE_STRING;
        _value_string  = s;
        _value_number  = null;
        _value_boolean = false;
    }

    public ConfigValue (Number n)
    {
        _type = CONFIG_TYPE_NUMBER;
        _value_string  = null;
        _value_number  = n;
        _value_boolean = false;
    }

    public ConfigValue (boolean b)
    {
        _type = CONFIG_TYPE_BOOLEAN;
        _value_string  = null;
        _value_number  = null;
        _value_boolean = b;
    }

    public int getType ()
    {
        return (_type);
    }

    public String getString ()
    {
        return (_value_string);
    }

    public Number getNumber ()
    {
        return (_value_number);
    }

    public boolean getBoolean ()
    {
        return (_value_boolean);
    }

    public String toString ()
    {
        if (_type == CONFIG_TYPE_STRING)
            return (_value_string);
        else if (_type == CONFIG_TYPE_NUMBER)
            return (_value_number.toString ());
        else if (_type == CONFIG_TYPE_BOOLEAN)
            return (Boolean.toString (_value_boolean));
        return (null);
    }
}
