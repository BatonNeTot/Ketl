/*🍲Ketl🍲*/
#ifndef garbage_collector_h
#define garbage_collector_h

#include <map>
#include <set>
#include <vector>
#include <unordered_set>

namespace Ketl {

	class Allocator;

	class GarbageCollector {
	public:

		GarbageCollector(Allocator& alloc)
			: _alloc(alloc) {}
		~GarbageCollector() {
			_refRoots.clear();
			collectGarbage();
		}

		using Finalizer = void(*)(void*);

		static void emptyFinalizer(void*) {};

		class RefHolder {
		public:

			inline RefHolder& registerAbsLink(const void* ptr) {
				_absLinks.emplace_back(ptr);
				return *this;
			}
			template <typename T>
			inline RefHolder& registerRefLink(const T* const* pptr) {
				_refLinks.emplace_back(reinterpret_cast<const void* const*>(pptr));
				return *this;
			}

			inline const auto& absLinks() const {
				return _absLinks;
			}
			inline const auto& refLinks() const {
				return _refLinks;
			}

		private:
			std::vector<const void*> _absLinks;
			std::vector<const void* const*> _refLinks;
		};

		RefHolder& registerMemory(void* ptr, size_t size, std::nullptr_t) = delete;
		RefHolder& registerMemory(void* ptr, size_t size, Finalizer finalizer = &emptyFinalizer) {
			return _objects.try_emplace(Object(_currUsageFlag, ptr, size, finalizer)).first->second;
		}

		template <typename T>
		inline void registerRefRoot(const T* const* pptr) {
			_refRoots.emplace(reinterpret_cast<const void* const*>(pptr));
		}
		template <typename T>
		inline void unregisterRefRoot(const T* const* pptr) {
			_refRoots.erase(reinterpret_cast<const void* const*>(pptr));
		}

		inline size_t collectGarbage() {
			if (_objects.empty()) {
				return 0u;
			}

			_currUsageFlag = !_currUsageFlag;
			markStage();
			return swipeStage();
		}

		inline void clearRoots() {
			_refRoots.clear();
		}

	private:

		void markStage();
		size_t swipeStage();

		void fillLinks(const std::set<const void*>& source, std::set<const void*>& target) const;

		class Object {
		public:

			Object(bool usageFlag, void* ptr, size_t size, Finalizer finalizer)
				: _usageFlag(usageFlag), _ptr(ptr), _size(size), _finalizer(finalizer) {}
			Object(const Object& other) = delete;
			Object(Object&& other) noexcept
				: _usageFlag(other._usageFlag), _ptr(other._ptr), _size(other._size), _finalizer(other._finalizer) {
				other._finalizer = &emptyFinalizer;
			}
			~Object();

			inline friend bool operator<(const Object& lhs, const Object& rhs) {
				return lhs._ptr < rhs._ptr;
			}

			inline bool getUsageFlag() const {
				return _usageFlag;
			}
			inline void updateUsageFlag(bool usageFlag) const {
				_usageFlag = usageFlag;
			}

			inline size_t size() const { return _size; }
			inline void* begin() const { return _ptr; }
			inline void* end() const { return reinterpret_cast<uint8_t*>(_ptr) + _size; }

		private:
			mutable bool _usageFlag;
			void* _ptr;
			size_t _size;
			Finalizer _finalizer;
		};

		bool _currUsageFlag = false;
		Allocator& _alloc;

		std::unordered_set<const void* const*> _refRoots;
		std::map<Object, RefHolder> _objects;

	};
}

#endif /*garbage_collector_h*/