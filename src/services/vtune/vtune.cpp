#include <ittnotify.h>

class ITTWrapper : public ToolWrapper {
    public:
    static itt_domain* domain;
    virtual std::string service_name(){
        return "VTune Service";
    }
    virtual std::string service_tag(){
        return "vtune";
    }
    virtual void beginAction(Caliper* c, const Attribute &attr, const Variant& value){
       __itt_task_begin(domain, __itt_null, __itt_null, __itt_string_handle_create(value.to_string()));
    }
    virtual void endAction(Caliper* c, const Attribute& attr, const Variant& value){
    }
    virtual void initalize(){
        domain = __itt_domain_create("Caliper Instrumentation Domain");
    }
}

CaliperService vtune_service { "vtune", &ITTWrapper::setCallbacks }
