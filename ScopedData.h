/*
        KStack Sample Application - ScopedData.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef SCOPED_DATA_H
#define SCOPED_DATA_H

#include <windows.h>
#include <cassert>

// RAII wrapper around Windows data
template<class Data = HANDLE, class ReleaseFuncReturn = BOOL>
class ScopedData
{
private:
	typedef ReleaseFuncReturn (WINAPI*DestroyFunc)(Data);
	typedef void (ScopedData::* bool_type)() const;

	DestroyFunc destroyer;
	Data data;

	ScopedData& operator=(const ScopedData&);
	ScopedData(const ScopedData& other);

	void NotComparable() const {}

public:
	ScopedData(const Data& data, const DestroyFunc& destroyer)
		: destroyer(destroyer),
		data(data)
	{
	}

	ScopedData() 
		: data(),
		destroyer(NULL)
	{}

	void Init(const Data& inputData, const DestroyFunc& destroyFunc)
	{
		assert(!destroyer && "ScopedData already initialized");
		data = inputData;
		destroyer = destroyFunc;
	}

	Data get() const
	{
		assert(destroyer && "ScopedData not initialized");
		return data;
	}

	Data operator*() const
	{
		return get();
	}

	// safe bool idiom
	operator bool_type() const
	{
		return (data) ? &ScopedData::NotComparable : 0;
	}

	~ScopedData()
	{
		destroyer ? (void)destroyer(data) : (void)0;
	}
};

template <class T, class U, class V> 
bool operator!=(const ScopedData<T, U>& lhs, const V& rhs) 
{
	lhs.NotComparable();	
	return false;
}

template <class T, class U, class V> 
bool operator==(const ScopedData<T, U>& lhs, const V& rhs) 
{
	lhs.NotComparable();	
	return false;
}

#endif