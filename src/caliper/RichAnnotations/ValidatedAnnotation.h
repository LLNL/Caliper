#include "../Annotation.h"
namespace cali{

template<class... ValidatorList>
class ValidatedAnnotation{
public:
    //using thisType = decltype(*this);
    using thisType = const ValidatedAnnotation<>&;
    //ValidatedAnnotation() : inner_annot("ERROR"){}
    ValidatedAnnotation(const char* name, int opt=0) : inner_annot(name,opt){
        inner_annot = Annotation(name,opt);
    }
    ValidatedAnnotation(const thisType& other) : inner_annot(other.getAnnot()) {
        inner_annot = other.getAnnot();
    }
    Annotation getAnnot() const{
        return inner_annot;
    }
    thisType& operator = (thisType & other){
        inner_annot = other.getAnnot();
        return *this;
    }
    thisType& begin(){
        inner_annot.begin();
        return *this;
    }
    thisType& begin(int data){
        inner_annot.begin(data);
        return *this;
    }
    thisType& begin(double data){
        inner_annot.begin(data);
        return *this;
    }
    thisType& begin(const char* data){
        inner_annot.begin(data);
        return *this;
    }
    thisType& begin(cali_attr_type type, void* data, uint64_t size){
        inner_annot.begin(type,data,size);
        return *this;
    }
    thisType& begin(const Variant& data){
        inner_annot.begin(data);
        return *this;
    }
    //set
    thisType& set(int data){
        inner_annot.set(data);
        return *this;
    }
    thisType& set(double data){
        inner_annot.set(data);
        return *this;
    }
    thisType& set(const char* data){
        inner_annot.set(data);
        return *this;
    }
    thisType& set(cali_attr_type type, void* data, uint64_t size){
        inner_annot.set(type,data,size);
        return *this;
    }
    thisType& set(const Variant& data){
        inner_annot.set(data);
        return *this;
    }
    void end(){
        inner_annot.end();
    }
private:
    Annotation inner_annot;
};

template<class Validator, class... ValidatorList>
class ValidatedAnnotation<Validator, ValidatorList...> : public ValidatedAnnotation<ValidatorList...>{
    public:
    #define DEBUG_RETURN_TYPE ValidatedAnnotation<Validator, ValidatorList...>
    using innerValidator = ValidatedAnnotation<ValidatorList...>;

    ValidatedAnnotation(const char* name, int opt=0) : ValidatedAnnotation<ValidatorList...>(name,opt){
       // ValidatedAnnotation<ValidatorList...>(name,opt);
    }
    ValidatedAnnotation(const DEBUG_RETURN_TYPE & other) : ValidatedAnnotation<ValidatorList...>(other){}
    //begin
    DEBUG_RETURN_TYPE &operator = (const DEBUG_RETURN_TYPE & other){
        my_val = other.getValidator();
        return *this;
    }
    DEBUG_RETURN_TYPE& begin(){
        innerValidator::begin();
        return *this;
    }
    DEBUG_RETURN_TYPE& begin(int data){
        innerValidator::begin(data);
        return *this;
    }
    DEBUG_RETURN_TYPE& begin(double data){
        innerValidator::begin(data);
        return *this;
    }
    DEBUG_RETURN_TYPE& begin(const char* data){
        innerValidator::begin(data);
        return *this;
    }
    DEBUG_RETURN_TYPE& begin(cali_attr_type type, void* data, uint64_t size){
        innerValidator::begin(type,data,size);
        return *this;
    }
    DEBUG_RETURN_TYPE& begin(const Variant& data){
        innerValidator::begin(data);
        return *this;
    }
    //set
    DEBUG_RETURN_TYPE& set(int data){
        innerValidator::set(data);
        return *this;
    }
    DEBUG_RETURN_TYPE& set(double data){
        innerValidator::set(data);
        return *this;
    }
    DEBUG_RETURN_TYPE& set(const char* data){
        innerValidator::set(data);
        return *this;
    }
    DEBUG_RETURN_TYPE& set(cali_attr_type type, void* data, uint64_t size){
        innerValidator::set(type,data,size);
        return *this;
    }
    DEBUG_RETURN_TYPE& set(const Variant& data){
        innerValidator::set(data);
        return *this;
    }
    void end(){
        innerValidator::end();
    }
    Validator my_val;
    Validator getValidator(){
        return my_val;
    }

};

struct dummy{};
} //end namespace cali
