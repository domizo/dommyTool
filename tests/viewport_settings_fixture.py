"""Extract settings and connection admission; traversal is a separate boundary."""
import re
from check_geometry_state_transactions import extract


def settings_bodies(root, paths, sources, sdk):
    code=['void observeSettings(bool);']
    guard=(root/'src/core/common/utility/DViewportCallbackGuard.h').read_text()
    macro=guard[guard.index('#define DOMMY_VIEWPORT_VOID_CATCH'):guard.index('#define DOMMY_VIEWPORT_POINTER_CATCH')]
    code += [macro.replace('DOMMY_VIEWPORT_VOID_CATCH','DOMMY_VIEWPORT_SETTINGS_VOID_CATCH',1)]
    # These are production statements up to the first input-data boundary,
    # not replacement geometry algorithms or complete callback runtime tests.
    selections=((0,'mMeshDisplayNode','        MObject obj;'),
                (1,'mDGNode','        MObject obj;'),
                (4,'mDGNode','        MObject obj;'),
                (5,'mCurveDisplayNode','    dommy::maya::ScopedPlugDataHandle curveHandleOwner;'),
                (6,'mDGNode','        dommy::maya::ScopedPlugDataHandle dCurveHndOwner;'))
    for index,node,boundary in selections:
        source=sources[index];name=paths[index].split('/')[-1]
        callback=extract(source,'void '+name+'Override::updateDG(')
        stage=callback[callback.index('{')+1:callback.index(boundary)]
        attributes=sorted(set(re.findall(name+r'::(\w+)',stage)))
        code += ['struct '+name+' {'+''.join('static MObject '+a+';' for a in attributes)+'};']
        if not sdk:
            for number,attribute in enumerate(attributes,100*(index+1)):
                code += ['MObject '+name+'::'+attribute+'{'+str(number)+'};']
        else:
            code += ['extern bool reachedSettingsData;']
        helpers=({'DMeshDisplay':('void clearMeshDisplayGeometry(',),
                  'DMMarker':('void clearMeshMarkerGroup(','void clearMeshMarkerGeometry('),
                  'DPMarker':('void clearPointMarkerGeometry(',),
                  'DCurveDisplay':(),
                  'DCMarker':('void clearMarkerGroup(','void clearMarkerGeometry(')})[name]
        code += [extract(source,signature) for signature in helpers]
        catch=(callback[re.search(r'catch\s*\(',callback).start():] if index != 5 else
               'DOMMY_VIEWPORT_SETTINGS_VOID_CATCH("DCurveDisplayOverride", "updateDG")')
        observed=''.join('observeSettings('+name+');' for name in ('isShowAttr','isShowGrp')
                         if re.search(r'bool\s+'+name+r'\s*=',stage))
        code += ['void settings'+name+'('+name+'Override::DrawData& dData) {',
                 'MObject '+node+'; bool mDataChanged=false,mMultiplierChanged=false; try {',
                 stage, observed+'reachedSettingsData=true;'+('}' if index != 5 else '')+'}',
                 catch,'(void)mDataChanged;(void)mMultiplierChanged;}']
    source=sources[2]
    evaluator=extract(source,'MStatus DPInstancerOverride::evalParms(')
    update=extract(source,'void DPInstancerOverride::updateDGImpl(')
    prefix=update[:update.index('dommy::maya::ScopedPlugDataHandle meshHndOwner;')]
    prefix=prefix.replace('DPInstancerOverride::updateDGImpl(', 'DPInstancerOverride::connectionAdmission(',1)
    from check_active_cpp_safety import strip_comments_and_literals
    clean=strip_comments_and_literals(prefix)
    admission=prefix+'reachedSettingsData=true;\n'+'}'*(clean.count('{')-clean.count('}'))
    if not sdk:
        attributes=sorted(set(re.findall(r'DPInstancer::(\w+)',evaluator+admission)))
        code += ['struct DPInstancer {'+''.join('static MObject '+a+';' for a in attributes)+'};']
        for number,attribute in enumerate(attributes,900):
            code += ['MObject DPInstancer::'+attribute+'{'+str(number)+'};']
        code += [extract(source,'MStatus readDataHandle('),
                 extract(source,'void DPInstancerOverride::clearDrawData('),
                 'void DPInstancerOverride::updateDGImpl(){if(useConnectionAdmission){connectionAdmission();return;}if(!api("instancer.update"))throw std::runtime_error("update");}']
    code += [evaluator,extract(source,'void DPInstancerOverride::updateDG(')]
    code += ['extern bool reachedSettingsData;' if sdk else '',admission]
    return '\n'.join(code)
