#include <ittnotify.h>

class ITTWrapper : public ToolWrapper<ITTWrapper> {
    public:
    static itt_domain* domain;
    static std::string service_name(){
        return "VTune Service";
    }
    static void beginAction(Caliper* c, const Attribute &attr, const Variant& value){
       __itt_task_begin(domain, __itt_null, __itt_null, __itt_string_handle_create(value.to_string()));
    }
    static void endAction(Caliper* c, const Attribute& attr, const Variant& value){
    }
    void initalize(){
        domain = __itt_domain_create("Caliper Instrumentation Domain");
    }
}

CaliperService ITTTriggerService { "vtune", &ITTWrapper::setCallbacks }
