#include "../Annotation.h"
#include <iostream>
#include <functional>
#include <type_traits>
#include "constraints/monotonic.h"
#include "constraints/bounded.h"
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
    using innerValidator = ValidatedAnnotation<ValidatorList...>;
    using wholeValidator = ValidatedAnnotation<Validator,ValidatorList...>;

    ValidatedAnnotation(const char* name, int opt=0) : ValidatedAnnotation<ValidatorList...>(name,opt){}
    ValidatedAnnotation(const wholeValidator & other) : ValidatedAnnotation<ValidatorList...>(other){}
    //begin
    wholeValidator &operator = (const wholeValidator & other){
        my_val = other.getValidator();
        return *this;
    }
    wholeValidator& begin(){
        my_val.validateBegin();
        innerValidator::begin();
        return *this;
    }
    wholeValidator& begin(int data){
        my_val.validateBegin(data);
        innerValidator::begin(data);
        return *this;
    }
    wholeValidator& begin(double data){
        my_val.validateBegin(data);
        innerValidator::begin(data);
        return *this;
    }
    wholeValidator& begin(const char* data){
        my_val.validateBegin(data);
        innerValidator::begin(data);
        return *this;
    }
    wholeValidator& begin(cali_attr_type type, void* data, uint64_t size){
        my_val.validateBegin(type,data,size);
        innerValidator::begin(type,data,size);
        return *this;
    }
    wholeValidator& begin(const Variant& data){
        my_val.validateBegin(data);
        innerValidator::begin(data);
        return *this;
    }
    //set
    wholeValidator& set(int data){
        my_val.validateSet(data);
        innerValidator::set(data);
        return *this;
    }
    wholeValidator& set(double data){
        my_val.validateSet(data);
        innerValidator::set(data);
        return *this;
    }
    wholeValidator& set(const char* data){
        my_val.validateSet(data);
        innerValidator::set(data);
        return *this;
    }
    wholeValidator& set(cali_attr_type type, void* data, uint64_t size){
        my_val.validateSet(type,data,size);
        innerValidator::set(type,data,size);
        return *this;
    }
    wholeValidator& set(const Variant& data){
        my_val.validateSet(data);
        innerValidator::set(data);
        return *this;
    }
    void end(){
        my_val.validateEnd();
        innerValidator::end();
    }
    Validator my_val;
    Validator getValidator(){
        return my_val;
    }

};

} //end namespace cali
