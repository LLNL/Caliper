#ifndef CALI_MONOTONIC_H
#define CALI_MONOTONIC_H
namespace cali {
template<typename T, typename Comparator>
struct Monotonic{
    public:
    void validateBegin(){}
    template<typename Q>
    typename std::enable_if<std::is_same<T,Q>::value,void>::type validateBegin(Q& next){
        if(Comparator()(next,last)){
            std::cout<<"ASPOLDE"<<std::endl;
        }
    }
    template<typename Q>
    typename std::enable_if<!std::is_same<T,Q>::value,void>::type validateBegin(Q& next){
    }
    template<typename Q>
    typename std::enable_if<std::is_same<T,Q>::value,void>::type validateSet(Q& next){
        if(Comparator()(next,last)){
            std::cout<<"ASPOLDE"<<std::endl;
        }
    }
    template<typename Q>
    typename std::enable_if<!std::is_same<T,Q>::value,void>::type validateSet(Q& next){
    }

    void validateEnd(){
    }
    private:
    T last;
};

template<typename T>
using MonotonicDecreasing = Monotonic<T,std::less_equal<T>>;
template<typename T>
using MonotonicIncreasing = Monotonic<T,std::greater_equal<T>>;
} //end namespace cali
#endif
