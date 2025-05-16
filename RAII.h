#ifndef __RAII_H__
#define __RAII_H__

namespace RAII
{
	template <class T>
	class ArrayAllocator
	{
		int &refs;
		T *array;
	public:
		ArrayAllocator(unsigned int numEls)
			: refs(*new int(1)),
				array(new T[numEls])
		{
		}

		ArrayAllocator(T *ptr)
			: refs(*new int(1)),
				array(ptr)
		{
		}

		ArrayAllocator(ArrayAllocator &other)
			: refs(other.refs),
				array(other.array)
		{
			refs++;
		}

		~ArrayAllocator()
		{
			refs--;
			if (!refs)
			{
				delete &refs;
				delete array;
			}
		}

		T *getArray()
		{
			return array;
		}
	};
}

#endif //(!__RAII_H__)

