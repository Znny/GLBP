//
// Created by Ryan on 8/30/2026.
//
#pragma once

#include <vector>

//single delegate - one subscriber, which receives the call
template<typename Ret, typename... Args>
class TDelegate
{
    using TrampolineFn = Ret(*)(void*, Args...);
    void* BoundObject = nullptr;
    TrampolineFn Trampoline;

public:

    template<typename T, Ret(T::*Function)(Args...)>
    void BindRaw(T* Object)
    {
        BoundObject = Object;
        Trampoline = [](void* Object, Args... args)
        {
            T* typedObject = static_cast<T*>(Object);
            return (typedObject->*Function)(args...);
        };
    }

    bool IsBound() const
    {
        return BoundObject != nullptr;
    }

    Ret Execute(Args... args)
    {
        return Trampoline(BoundObject, args...);
    }

};

#define DECLARE_DELEGATE(DelegateName) \
    typedef TDelegate<void> DelegateName;

#define DECLARE_DELEGATE_1Param(DelegateName, Param1) \
    typedef TDelegate<void, Param1> DelegateName;

#define DECLARE_DELEGATE_2Param(DelegateName, Param1, Param2) \
    typedef TDelegate<void, Param1, Param2> DelegateName;


//multicast delegates - more than one subscriber, broadcast to all
template<typename Ret, typename... Args>
class TDelegateMulticast
{
    std::vector<TDelegate<Ret, Args...>> invocationList;
public:
    template<typename T, Ret(T::*memFn)(Args...)>
    void Add(T* Obj)
    {
        TDelegate<Ret, Args...> newDg;
        newDg.template BindRaw<T, memFn>(Obj);

        invocationList.push_back(newDg);
    }

    void Broadcast(Args... args)
    {
        for( auto& dg : invocationList)
        {
            dg.Execute(args...);
        }
    }
};

#define DECLARE_MULTICAST_DELEGATE(DelegateName) \
    typedef TDelegateMulticast<void> DelegateName;

#define DECLARE_MULTICAST_DELEGATE_1Param(DelegateName, Param1) \
    typedef TDelegateMulticast<void, Param1> DelegateName;

#define DECLARE_MULTICAST_DELEGATE_2Param(DelegateName, Param1, Param2) \
    typedef TDelegateMulticast<void, Param1, Param2> DelegateName;
