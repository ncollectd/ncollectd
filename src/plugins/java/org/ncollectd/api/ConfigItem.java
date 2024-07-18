// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.api;

import java.util.List;
import java.util.ArrayList;

/**
 * Java representation of ncollectd/src/libconfig/config.h:config_item_t structure.
 *
 * @author Florian Forster &lt;octo at collectd.org&gt;
 */
public class ConfigItem
{
    private String _key = null;
    private List<ConfigValue> _values   = new ArrayList<ConfigValue> ();
    private List<ConfigItem>  _children = new ArrayList<ConfigItem> ();

    public ConfigItem (String key)
    {
        this._key = key;
    }

    public String getKey ()
    {
        return this._key;
    }

    public void addValue (ConfigValue cv)
    {
        this._values.add(cv);
    }

    public void addValue (String s)
    {
        this._values.add(new ConfigValue (s));
    }

    public void addValue (Number n)
    {
        this._values.add(new ConfigValue (n));
    }

    public void addValue (boolean b)
    {
        this._values.add(new ConfigValue (b));
    }

    public List<ConfigValue> getValues ()
    {
        return this._values;
    }

    public void addChild (ConfigItem ci)
    {
        this._children.add(ci);
    }

    public List<ConfigItem> getChildren ()
    {
        return this._children;
    }

    public String toString ()
    {
        return (new String ("{ key: " + _key + "; "
                    + "values: " + this._values.toString () + "; "
                    + "children: " + this._children.toString () + "; }"));
    }
}
