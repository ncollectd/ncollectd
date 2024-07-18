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

    private int     _type;
    private String  _value_string;
    private Number  _value_number;
    private boolean _value_boolean;

    public ConfigValue (String s)
    {
        this._type = CONFIG_TYPE_STRING;
        this._value_string  = s;
        this._value_number  = null;
        this._value_boolean = false;
    }

    public ConfigValue (Number n)
    {
        this._type = CONFIG_TYPE_NUMBER;
        this._value_string  = null;
        this._value_number  = n;
        this._value_boolean = false;
    }

    public ConfigValue (boolean b)
    {
        this._type = CONFIG_TYPE_BOOLEAN;
        this._value_string  = null;
        this._value_number  = null;
        this._value_boolean = b;
    }

    public int getType ()
    {
        return this._type;
    }

    public String getString ()
    {
        return this._value_string;
    }

    public Number getNumber ()
    {
        return this._value_number;
    }

    public boolean getBoolean ()
    {
        return this._value_boolean;
    }

    public String toString ()
    {
        if (this._type == CONFIG_TYPE_STRING)
            return (this._value_string);
        else if (this._type == CONFIG_TYPE_NUMBER)
            return (this._value_number.toString ());
        else if (this._type == CONFIG_TYPE_BOOLEAN)
            return (Boolean.toString (this._value_boolean));
        return (null);
    }
}
