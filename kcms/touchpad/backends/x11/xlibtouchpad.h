/*
    SPDX-FileCopyrightText: 2015 Weng Xuetian <wengxt@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QSet>
#include <QVariantHash>

#include "propertyinfo.h"
#include "xcbatom.h"
#include <xcb/xcb.h>

enum ParaType {
    PT_INT,
    PT_BOOL,
    PT_DOUBLE,
};

struct Parameter {
    const char *name; /* Name of parameter */
    enum ParaType type; /* Type of parameter */
    double min_val; /* Minimum allowed value */
    double max_val; /* Maximum allowed value */
    const char *prop_name; /* Property name */
    int prop_format; /* Property format (0 for floats) */
    unsigned prop_offset; /* Offset inside property */
};

class XlibTouchpad
{
public:
    XlibTouchpad(Display *display, int deviceId);
    virtual ~XlibTouchpad()
    {
    }

    int deviceId()
    {
        return m_deviceId;
    }
    virtual bool load()
    {
        return false;
    }
    virtual bool save()
    {
        return false;
    }
    virtual bool defaults()
    {
        return false;
    }
    virtual bool isSaveNeeded() const
    {
        return false;
    }
    void setSuspended(bool suspended);
    bool isSuspended();

    virtual XcbAtom &touchpadOffAtom() = 0;

protected:
    void loadSupportedProperties(const Parameter *props);
    bool setParameter(const struct Parameter *, const QVariant &);
    QVariant getParameter(const struct Parameter *);
    struct PropertyInfo *getDevProperty(const QLatin1String &propName);
    void flush();
    const Parameter *findParameter(const QString &name);

    Display *m_display;
    xcb_connection_t *m_connection;
    int m_deviceId;

    XcbAtom m_floatType, m_enabledAtom;

    QMap<QLatin1String, std::shared_ptr<XcbAtom>> m_atoms;

    QMap<QLatin1String, struct PropertyInfo> m_props;
    QSet<QLatin1String> m_changed;
    const struct Parameter *m_paramList;
};
