// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// validator.cpp
// Caliper annotation nesting validator

#include "caliper/AnnotationBinding.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "caliper/reader/Expand.h"

#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

using namespace cali;


namespace
{

std::ostream& print_snapshot(Caliper* c, Channel* chn, std::ostream& os)
{
    SnapshotRecord::FixedSnapshotRecord<80> snapshot_data;
    SnapshotRecord snapshot(snapshot_data);

    c->pull_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr, &snapshot);

    os << "{ ";

    OutputStream stream;
    stream.set_stream(&os);

    cali::Expand exp(stream, "");
    exp.process_record(*c, snapshot.to_entrylist());

    return os << " }";
}

class ValidatorService
{
    static Attribute s_class_nested_attr;

    class StackValidator
    {
        std::map< Attribute, std::vector<Variant> > m_region_stack;
        bool m_error_found;
        Node m_root_node;

        void push(Caliper* c, const Attribute& attr, const Variant& value) {
            Variant v_copy = value;

            if (!attr.store_as_value())
                v_copy = c->make_tree_entry(attr, value, &m_root_node)->data();

            m_region_stack[attr].push_back(v_copy);
        }

        Variant pop(const Attribute& attr) {
            Variant ret;

            auto it = m_region_stack.find(attr);
            if (it == m_region_stack.end())
                return ret;
            if (it->second.empty())
                return ret;

            ret = it->second.back();
            it->second.pop_back();

            if (it->second.empty())
                m_region_stack.erase(it);

            return ret;
        }

    public:

        bool check_begin(Caliper* c, const Attribute& attr, const Variant& value) {
            if (m_error_found)
                return true;

            push(c, attr, value);

            if (attr.is_nested())
                push(c, s_class_nested_attr, Variant(attr.id()));

            return false;
        }

        bool check_end(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
            if (m_error_found)
                return true;

            Variant v_stack_val = pop(attr);

            if (v_stack_val.empty()) {
                // We currently can't actually check this situation because
                // the Caliper runtime prevents events being executed in
                // "empty stack" situations :-/

                m_error_found = true;

                print_snapshot(c, chn,
                               Log(0).stream() << "validator: end(\""
                               << attr.name() << "\"=\"" << value.to_string() << "\") "
                               << " has no matching begin().\n    context: " )
                    << std::endl;
            } else {
                Variant v_stack_attr;

                if (attr.is_nested())
                    v_stack_attr = pop(s_class_nested_attr);

                if (attr.is_nested() && attr.id() != v_stack_attr.to_id()) {
                    m_error_found = true;

                    print_snapshot(c, chn,
                                   Log(0).stream() << "validator: incorrect nesting: trying to end \""
                                   << attr.name() << "\"=\"" << value.to_string()
                                   << "\" but current attribute is \""
                                   << c->get_attribute(v_stack_attr.to_id()).name()
                                   << "\".\n    context: " )
                        << std::endl;
                } else if (!(value == v_stack_val)) {
                    m_error_found = true;

                    print_snapshot(c, chn,
                                   Log(0).stream() << "validator: incorrect nesting: trying to end \""
                                   << attr.name() << "\"=\"" << value.to_string()
                                   << "\" but current value is \""
                                   << v_stack_val.to_string() << "\".\n    context: " )
                        << std::endl;
                }
            }

            return m_error_found;
        }

        bool check_final() {
            for (auto const &p : m_region_stack) {
                if (p.first == s_class_nested_attr)
                    continue;

                if (!p.second.empty()) {
                    std::ostringstream os;

                    os << "validator: Regions not closed: "
                       << p.first.name() << "=";

                    int cv = 0;
                    for (const Variant& v : p.second)
                        os << (cv++ > 0 ? "/" : "") << v.to_string();

                    Log(0).stream() << os.str() << std::endl;

                    m_error_found = true;
                }
            }

            return m_error_found;
        }

        StackValidator()
            : m_error_found(false), m_root_node(CALI_INV_ID, CALI_INV_ID, Variant())
            { }
    }; // class StackValidator


    StackValidator*   proc_stack = nullptr;
    std::mutex        proc_stack_mutex;

    std::atomic<int>  global_errors;

    Attribute         thread_stack_attr;

    std::vector<StackValidator*> thread_stacks;
    std::mutex        thread_stacks_mutex;


    StackValidator* aquire_thread_stack(Caliper* c, Channel* chn) {
        StackValidator* tstack =
            static_cast<StackValidator*>(c->get(thread_stack_attr).value().get_ptr());

        if (!tstack) {
            tstack = new StackValidator;

            c->set(thread_stack_attr, Variant(cali_make_variant_from_ptr(tstack)));

            std::lock_guard<std::mutex>
                g(thread_stacks_mutex);

            thread_stacks.push_back(tstack);
        }

        return tstack;
    }

    void finalize_cb(Caliper*, Channel* chn) {
        {
            std::lock_guard<std::mutex>
                g(proc_stack_mutex);

            if (proc_stack->check_final())
                ++global_errors;

            delete proc_stack;
            proc_stack = nullptr;
        }

        {
            std::lock_guard<std::mutex>
                g(thread_stacks_mutex);

            for (StackValidator* v : thread_stacks) {
                if (v->check_final())
                    ++global_errors;

                delete v;
            }

            thread_stacks.clear();
        }

        if (global_errors.load() > 0)
            Log(0).stream() << "validator: Annotation nesting errors found"
                            << std::endl;
        else
            Log(1).stream() << "validator: No annotation nesting errors found"
                            << std::endl;
    }

    void begin_cb(Caliper* c, Channel* chn, const Attribute &attr, const Variant& value) {
        if ((attr.properties() & CALI_ATTR_SCOPE_MASK) == CALI_ATTR_SCOPE_PROCESS) {
            std::lock_guard<std::mutex>
                g(proc_stack_mutex);

            if (proc_stack->check_begin(c, attr, value))
                ++global_errors;
        } else {
            StackValidator* v = aquire_thread_stack(c, chn);

            if (v && v->check_begin(c, attr, value))
                ++global_errors;
        }
    }

    void end_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        if ((attr.properties() & CALI_ATTR_SCOPE_MASK) == CALI_ATTR_SCOPE_PROCESS) {
            std::lock_guard<std::mutex>
                g(proc_stack_mutex);

            if (proc_stack->check_end(c, chn, attr, value))
                ++global_errors;
        } else {
            StackValidator* v = aquire_thread_stack(c, chn);

            if (v && v->check_end(c, chn, attr, value))
                ++global_errors;
        }
    }

    ValidatorService(Caliper* c, Channel* chn)
            : proc_stack(new StackValidator), global_errors(0)
        {
            thread_stack_attr =
                c->create_attribute(std::string("validator.stack.")+std::to_string(chn->id()),
                                    CALI_TYPE_PTR,
                                    CALI_ATTR_SCOPE_THREAD |
                                    CALI_ATTR_ASVALUE      |
                                    CALI_ATTR_SKIP_EVENTS  |
                                    CALI_ATTR_HIDDEN);
        }

public:

    static void validator_register(Caliper* c, Channel* chn) {
        if (s_class_nested_attr == Attribute::invalid)
            s_class_nested_attr =
                c->create_attribute("validator.nested", CALI_TYPE_UINT, CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN);

        ValidatorService* instance = new ValidatorService(c, chn);

        chn->events().pre_begin_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr, const Variant& value){
                instance->begin_cb(c, chn, attr, value);
            });
        chn->events().pre_end_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr, const Variant& value){
                instance->end_cb(c, chn, attr, value);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->finalize_cb(c, chn);
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered validator service." << std::endl;
    }

};

Attribute ValidatorService::s_class_nested_attr { Attribute::invalid };

} // namespace [anonymous]


namespace cali
{

CaliperService validator_service { "validator", ::ValidatorService::validator_register };

}
