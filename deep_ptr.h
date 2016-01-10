#include <type_traits>
#include <cassert>

template <typename T> struct control_block {
  virtual ~control_block() = default;
  virtual std::unique_ptr<control_block> clone() const = 0;
  virtual T *ptr() = 0;
};

template <typename T, typename U>
class control_block_impl : public control_block<T> {
  U *p_ = nullptr;

public:
  explicit control_block_impl(U *p) : p_(p) {}

  ~control_block_impl() { delete p_; }

  std::unique_ptr<control_block<T>> clone() const override {
    return std::make_unique<control_block_impl>(new U(*p_));
  }

  T *ptr() override { return p_; }
};

template <typename T, typename U>
class delegating_control_block : public control_block<T> {

  std::unique_ptr<control_block<U>> delegate_;

public:
  explicit delegating_control_block(std::unique_ptr<control_block<U>> b)
      : delegate_(std::move(b)) {}

  std::unique_ptr<control_block<T>> clone() const override {
    return std::make_unique<delegating_control_block>(delegate_->clone());
  }

  T *ptr() override { return delegate_->ptr(); }
};

template <typename T> class deep_ptr {

  template <typename U> friend class deep_ptr;

  T *ptr_ = nullptr;
  std::unique_ptr<control_block<T>> cb_;

public:
  ~deep_ptr() = default;

  //
  // Constructors
  //

  deep_ptr() {}

  deep_ptr(std::nullptr_t) : deep_ptr() {}

  template <typename U,
            typename V = std::enable_if_t<std::is_base_of<T, U>::value>>
  explicit deep_ptr(U *u) {
    if (!u) {
      return;
    }

    cb_ = std::make_unique<control_block_impl<T,U>>(u);
    ptr_ = u;
  }

  //
  // Copy-constructors
  //

  deep_ptr(const deep_ptr &p) {
    if (!p) {
      return;
    }
    auto tmp_cb = p.cb_->clone();
    ptr_ = tmp_cb->ptr();
    cb_ = std::move(tmp_cb);
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_base_of<T, U>::value>>
  deep_ptr(const deep_ptr<U> &p) {
    deep_ptr<U> tmp(p);
    ptr_ = tmp.ptr_;
    cb_ = std::make_unique<delegating_control_block<T,U>>(std::move(tmp.cb_));
  }

  //
  // Move-constructors
  //

  deep_ptr(deep_ptr &&p)
  {
    ptr_ = p.ptr_;
    cb_ = std::move(p.cb_);
    p.ptr_ = nullptr;
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_base_of<T, U>::value>>
  deep_ptr(deep_ptr<U> &&p) {
    ptr_ = p.ptr_;
    cb_ = std::make_unique<delegating_control_block<T,U>>(std::move(p.cb_));
    p.ptr_ = nullptr;
  }

  //
  // Assignment
  //

  deep_ptr &operator=(const deep_ptr &p) {
    if (&p == this) {
      return *this;
    }

    if(!p)
    {
      cb_.reset();
      ptr_ = nullptr;
      return *this;
    }

    auto tmp_cb = p.cb_->clone();
    ptr_ = tmp_cb->ptr();
    cb_ = std::move(tmp_cb);
    return *this;
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_base_of<T, U>::value>>
  deep_ptr &operator=(const deep_ptr<U> &p) {
    deep_ptr<U> tmp(p);
    *this = std::move(tmp);
    return *this;
  }

  //
  // Move-assignment
  //

  deep_ptr &operator=(deep_ptr &&p)
  {                  
    if (&p == this) {
      return *this;
    }
    
    cb_ = std::move(p.cb_);
    ptr_ = p.ptr_;
    p.ptr_ = nullptr;
    return *this;
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_base_of<T, U>::value>>
  deep_ptr &operator=(deep_ptr<U> &&p) {
    cb_ = std::make_unique<delegating_control_block<T,U>>(std::move(p.cb_));
    ptr_ = p.ptr_;
    p.ptr_ = nullptr;
    return *this;
  }

  //
  // Accessors
  //

  const operator bool() const { return ptr_; }

  const T *operator->() const { return get(); }

  const T *get() const { return ptr_; }

  const T &operator*() const {
    assert(ptr_);
    return *get();
  }

  //
  // Non-const accessors
  //

  T *operator->() { return get(); }

  T *get() { return ptr_; }

  T &operator*() {
    assert(ptr_);
    return *get();
  }
};

template <typename T, typename... Ts> deep_ptr<T> make_deep_ptr(Ts &&... ts) {
  return deep_ptr<T>(new T(std::forward<Ts>(ts)...));
}

