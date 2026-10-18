#pragma once
#include "oscilloscopeconfiguration.h"
#include "oscilloscope.h"
#include "msoconfiguration.h"
inline bool buildOscilloscopeCommands(const QVariantMap &cfg,const QString &model,int channels,
                                     QStringList &commands,QString &error) {
    commands.clear();
    OscilloscopeSettings settings;
    if(!prepareOscilloscopeSettings(cfg,model,channels,settings,error)) return false;
    commands=compileMsoConfiguration(settings);
    return true;
}
inline bool applyOscilloscopeConfiguration(Oscilloscope *scope,const QVariantMap &cfg,QAtomicInt &stop,QString &error) {
    if(stop.loadAcquire()) {error="Stopped by user";return false;}
    OscilloscopeSettings settings;
    return prepareOscilloscopeSettings(cfg,scope->model(),scope->getTotalChannel(),settings,error)
        && scope->applySettings(settings,stop,error);
}
