#ifndef CALI_BOUNDED_H
#define CALI_BOUNDED_H
namespace cali {
template<typename T, typename Comparator, T bound>
class Bounded
{
    public:
    void validateBegin(){}
    template<typename Q>
    typename std::enable_if<std::is_same<T,Q>::value,void>::type validateBegin(Q& next){
        if(Comparator()(bound,next)){
            std::cout<<"ASPOLDE 2"<<std::endl;
        }
    }
    void validateBegin(cali_attr_type type, void* data, uint64_t size){
    }
    template<typename Q>
    typename std::enable_if<!std::is_same<T,Q>::value,void>::type validateBegin(Q& next){
    }
    template<typename Q>
    typename std::enable_if<std::is_same<T,Q>::value,void>::type validateSet(Q& next){
        if(Comparator()(bound,next)){
            std::cout<<"ASPOLDE 2"<<std::endl;
        }
    }
    template<typename Q>
    typename std::enable_if<!std::is_same<T,Q>::value,void>::type validateSet(Q& next){
    }

    void validateSet(cali_attr_type type, void* data, uint64_t size){
    }
    void validateEnd(){
    }
};

template<typename T, T bound>
using BoundedAbove = Bounded<T,std::less_equal<T>,bound>;
template<typename T, T bound>
using BoundedBelow = Bounded<T,std::greater_equal<T>,bound>;

} //end namespace cali
#endif
