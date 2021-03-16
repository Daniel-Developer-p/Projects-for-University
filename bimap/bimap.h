#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
  typename CompareRight = std::less<Right>>
  class bimap
{
  class Node;
  class Val_node;

  template <typename Ptr, typename Cmp>
  struct compressed_pair_impl : public Cmp
  {
    Ptr pointer;

    compressed_pair_impl(const Ptr& ptr, const Cmp& cmp)
      : Cmp(cmp), pointer(ptr)
    {}

    compressed_pair_impl(compressed_pair_impl&& other) noexcept
      : Cmp(std::move(other)), pointer(std::move(other.pointer))
    {}

    compressed_pair_impl(const compressed_pair_impl& other)
      : Cmp(other), pointer(other.pointer)
    {}

    compressed_pair_impl& operator=(compressed_pair_impl&& other) noexcept
    {
      if (this == &other) { return *this; }

      static_cast<Cmp&>(*this) = std::move(static_cast<Cmp&>(other));
      pointer = std::move(other.pointer);
      return *this;
    }

    compressed_pair_impl& operator=(const compressed_pair_impl& other)
    {
      if (this == &other) { return *this; }

      compressed_pair_impl backup = std::move(*this);
      try
      {
        static_cast<Cmp&>(*this) = static_cast<Cmp&>(other);
        pointer = other.pointer;
      }
      catch (...)
      {
        *this = std::move(backup);
        throw;
      }

      return *this;
    }
  };

public:
  using left_t = Left;
  using right_t = Right;

  using node_t = Node;
  using node_ptr = Node *;

  template <bool is_left>
  struct iterator
    : public std::iterator<std::input_iterator_tag, std::conditional_t<is_left, Left, Right>> 
  {
    using iter = iterator<is_left>;
    using val_type = std::conditional_t<is_left, Left, Right>;
    friend bimap;

    iterator(node_ptr ptr) : ptr(ptr) {}

    val_type const& operator*() const { return get<is_left>(ptr); }

    iter& operator++()
    {
      ptr = Node::template next<is_left>(ptr);
      return *this;
    }
    iter operator++(int)
    {
      iter copy = *this;
      ++(*this);
      return copy;
    }

    iter& operator--()
    {
      ptr = Node::template prev<is_left>(ptr);
      return *this;
    }
    iter operator--(int)
    {
      iter copy = *this;
      --(*this);
      return copy;
    }

    iterator<!is_left> flip() const { return iterator<!is_left>(ptr); }

    friend bool operator==(const iter& first, const iter& second)
    {
      return first.ptr == second.ptr;
    }

    friend bool operator!=(const iter& first, const iter& second)
    {
      return !(first == second);
    }

  private:
    node_ptr ptr;
  };

  using left_iterator = iterator<true>;
  using right_iterator = iterator<false>;

  // Создает bimap не содержащий ни одной пары.
  bimap(CompareLeft compare_left = CompareLeft(),
    CompareRight compare_right = CompareRight())
    : root(new Node())
    , left_pair(root.get(), compare_left)
    , right_pair(root.get(), compare_right)
    , node_cnt(0)
    , is_changed(false)
  {}

  // Конструкторы от других и присваивания
  bimap(bimap const& other)
    : root(new Node())
    , left_pair(other.left_pair)
    , right_pair(other.right_pair)
    , node_cnt(0)
    , is_changed(true) // Потому что сейчас у нас "чужие" begin_left и begin_right
  {
    insert_range(other.begin_left(), other.end_left(), other.size());
  }

  bimap(bimap&& other) noexcept
    : root(std::move(other.root))
    , left_pair(std::move(other.left_pair))
    , right_pair(std::move(other.right_pair))
    , node_cnt(other.node_cnt)
    , is_changed(other.is_changed)
  {
    other.node_cnt = 0;
    other.left_pair.pointer = other.right_pair.pointer = other.root.get();
  }

  bimap& operator=(bimap&& other) noexcept
  {
    if (this == &other) { return *this; }

    clear();
    root = std::move(other.root);
    left_pair = std::move(other.left_pair);
    right_pair = std::move(other.right_pair);
    node_cnt = other.node_cnt;
    is_changed = other.is_changed;
    other.node_cnt = 0;

    return *this;
  }
  bimap& operator=(bimap const& other)
  {
    if (this == &other) { return *this; }

    bimap<left_t, right_t, CompareLeft, CompareRight> backup = std::move(*this);
    clear();
    is_changed = true; // Потому что сейчас у нас "чужие" begin_left и begin_right
    try
    {
      insert_range(other.begin_left(), other.end_left(), other.size());
    }
    catch (...)
    {
      *this = std::move(backup);
      throw;
    }
    return *this;
  }

  // Деструктор. Вызывается при удалении объектов bimap.
  // Инвалидирует все итераторы ссылающиеся на элементы этого bimap
  // (включая итераторы ссылающиеся на элементы следующие за последними).
  ~bimap() { clear(); }

  // Вставка пары (left, right), возвращает итератор на left.
  // Если такой left или такой right уже присутствуют в bimap, вставка не
  // производится и возвращается end_left().
  left_iterator insert(left_t const& left, right_t const& right)
  {
    return emplace(left, right, left, right);
  }
  left_iterator insert(left_t const& left, right_t&& right)
  {
    return emplace(left, right, left, std::move(right));
  }
  left_iterator insert(left_t&& left, right_t const& right)
  {
    return emplace(left, right, std::move(left), right);
  }
  left_iterator insert(left_t&& left, right_t&& right)
  {
    return emplace(left, right, std::move(left), std::move(right));
  }

  // Удаляет элемент и соответствующий ему парный.
  // erase невалидного итератора неопределен.
  // erase(end_left()) и erase(end_right()) неопределены.
  // Пусть it ссылается на некоторый элемент e.
  // erase инвалидирует все итераторы ссылающиеся на e и на элемент парный к e.
  left_iterator erase_left(left_iterator it)
  {
    if (it == end_left()) { return it; }

    node_ptr copy = it.ptr;
    node_ptr left_par = copy->template parent<true>();
    node_ptr right_par = copy->template parent<false>();
    it++;
    node_ptr left_merge_res = merge_left(copy->template go_left<true>(), copy->template go_right<true>());
    node_ptr right_merge_res = merge_right(copy->template go_left<false>(), copy->template go_right<false>());
    copy->nullify();
    if (left_par)
    {
      if (copy == left_par->template go_left<true>())
      {
        left_par->template attach_left<true>(left_merge_res);
      }
      else
      {
        left_par->template attach_right<true>(left_merge_res);
      }
    }

    if (right_par)
    {
      if (copy == right_par->template go_left<false>())
      {
        right_par->template attach_left<false>(right_merge_res);
      }
      else
      {
        right_par->template attach_right<false>(right_merge_res);
      }
    }

    delete copy;
    node_cnt--;
    is_changed = true;
    return it;
  }
  right_iterator erase_right(right_iterator it)
  {
    right_iterator copy = it++;
    erase_left(copy.flip());
    return it;
  }

  // Аналогично erase, но по ключу, удаляет элемент если он присутствует, иначе
  // не делает ничего Возвращает была ли пара удалена
  bool erase_left(left_t const& left) { return erase<true>(left); }
  bool erase_right(right_t const& right) { return erase<false>(right); }

  // erase от ренжа, удаляет [first, last), возвращает итератор на последний
  // элемент за удаленной последовательностью
  left_iterator erase_left(left_iterator first, left_iterator last)
  {
    return erase<true>(first, last);
  }
  right_iterator erase_right(right_iterator first, right_iterator last)
  {
    return erase<false>(first, last);
  }

  // Возвращает итератор по элементу. Если не найден - соответствующий end()
  left_iterator find_left(left_t const& left) const
  {
    node_ptr temp_it = find_node_left(left);
    if (temp_it) 
      return left_iterator(temp_it);
    return end_left();
  }
  right_iterator find_right(right_t const& right) const
  {
    node_ptr temp_it = find_node_right(right);
    if (temp_it) 
      return right_iterator(temp_it);
    return end_right();
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует -- бросает std::out_of_range

  right_t const& at_left(left_t const& key) const { return at<true>(key); }
  left_t const& at_right(right_t const& key) const { return at<false>(key); }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует, добавляет его в bimap и на противоположную
  // сторону кладет дефолтный элемент, ссылку на который и возвращает
  // Если дефолтный элемент уже лежит в противоположной паре - должен поменять
  // соответствующий ему элемент на запрашиваемый 
  template <class = typename std::enable_if<std::is_default_constructible_v<right_t>>>
  right_t const& at_left_or_default(left_t const& key)
  {
    return at_or_default<true>(key);
  }
  template <class = typename std::enable_if<std::is_default_constructible_v<left_t>>>
  left_t const& at_right_or_default(right_t const& key)
  {
    return at_or_default<false>(key);
  }

  // lower и upper bound'ы по каждой стороне
  // Возвращают итераторы на соответствующие элементы
  // Смотри std::lower_bound, std::upper_bound.
  left_iterator lower_bound_left(const left_t& key) const
  {
    return lower_bound<true>(key, static_cast<CompareLeft>(left_pair));
  }
  right_iterator lower_bound_right(const right_t& key) const
  {
    return lower_bound<false>(key, static_cast<CompareRight>(right_pair));
  }

  left_iterator upper_bound_left(const left_t& key) const
  {
    return upper_bound<true>(key, static_cast<CompareLeft>(left_pair));
  }
  right_iterator upper_bound_right(const right_t& key) const
  {
    return upper_bound<false>(key, static_cast<CompareRight>(right_pair));
  }

  // Возващает итератор на минимальный по порядку left.
  left_iterator begin_left() const
  {
    if (is_changed) 
      recalc();
    return left_iterator(left_pair.pointer);
  }
  // Возващает итератор на следующий за последним по порядку left.
  left_iterator end_left() const { return left_iterator(root.get()); }

  // Возващает итератор на минимальный по порядку right.
  right_iterator begin_right() const
  {
    if (is_changed) 
      recalc();
    return right_iterator(right_pair.pointer);
  }
  // Возващает итератор на следующий за последним по порядку right.
  right_iterator end_right() const { return right_iterator(root.get()); }

  void clear()
  {
    erase_left(begin_left(), end_left());
    root.reset(new Node());
    is_changed = false;
    node_cnt = 0;
  }

  // Проверка на пустоту
  bool empty() const { return node_cnt == 0; }

  // Возвращает размер бимапы (кол-во пар)
  std::size_t size() const { return node_cnt; }

  // операторы сравнения
  friend bool operator==(bimap const& a, bimap const& b)
  {
    left_iterator a_curr = a.begin_left(), b_curr = b.begin_left();
    while (a_curr != a.end_left() && b_curr != b.end_left())
    {
      if (*a_curr != *b_curr || *(a_curr.flip()) != *(b_curr.flip()))
      {
        return false;
      }
      a_curr++;
      b_curr++;
    }
    if (a_curr != a.end_left() || b_curr != b.end_left())
    {
      return false;
    }
    return true;
  }
  friend bool operator!=(bimap const& a, bimap const& b) { return !(a == b); }

private:
  std::unique_ptr<Node> root;
  mutable compressed_pair_impl<node_ptr, CompareLeft> left_pair;
  mutable compressed_pair_impl<node_ptr, CompareRight> right_pair;
  size_t node_cnt;
  mutable bool is_changed;

  template <bool is_left>
  static std::conditional_t<is_left, const left_t&, const right_t&> get(const node_ptr node)
  {
    Val_node* temp_cast = static_cast<Val_node*>(node);
    if constexpr (is_left)
      return temp_cast->left_val;
    else
      return temp_cast->right_val;
  }

  template <bool is_left>
  using val_type = std::conditional_t<is_left, left_t, right_t>;
  template <bool is_left>
  using cmp_type = std::conditional_t<is_left, CompareLeft, CompareRight>;

  template <bool is_left>
  node_ptr const find_node(const val_type<is_left>& key,
    const cmp_type<is_left>& cmp) const
  {
    node_ptr curr = root->template go_left<is_left>();
    while (curr)
    {
      if (cmp(get<is_left>(curr), key))
        curr = curr->template go_right<is_left>();
      else if (cmp(key, get<is_left>(curr)))
        curr = curr->template go_left<is_left>();
      else
        return curr;
    }
    return nullptr;
  }
  node_ptr const find_node_left(const left_t& key) const
  {
    return find_node<true>(key, static_cast<CompareLeft>(left_pair));
  }
  node_ptr const find_node_right(const right_t& key) const
  {
    return find_node<false>(key, static_cast<CompareRight>(right_pair));
  }

  void insert_range(const left_iterator other_begin, const left_iterator other_end, int size)
  {
    size_t count = 0;
    left_iterator pivot1 = other_begin;
    while (count != size / 2 && pivot1 != other_end)
    {
      pivot1++;
      count++;
    }
    if (pivot1 == other_end) 
      pivot1--;

    insert(*pivot1, *(pivot1.flip()));
    size--;
    left_iterator pivot2 = pivot1--;
    pivot2++;
    while (pivot2 != other_end && size > 0)
    {
      insert(*pivot2, *(pivot2.flip()));
      pivot2++;
      size--;
    }
    while (pivot1 != other_begin && size > 0)
    {
      insert(*pivot1, *(pivot1.flip()));
      pivot1--;
      size--;
    }
    if (size > 0) 
      insert(*pivot1, *(pivot1.flip()));
  }

  template <class... Args>
  left_iterator emplace(const left_t& left_key, const right_t& right_key, Args&& ... args)
  {
    if (find_left(left_key) == end_left() &&
      find_right(right_key) == end_right())
    {
      node_ptr to_insert = new Val_node(std::forward<Args>(args)...);
      std::pair<node_ptr, node_ptr> split_res = split_left(root->template go_left<true>(), left_key);
      node_ptr left_merge_res = merge_left(merge_left(split_res.first, to_insert), split_res.second);
      root->template attach_left<true>(left_merge_res);

      split_res = split_right(root->template go_left<false>(), right_key);
      node_ptr right_merge_res = merge_right(merge_right(split_res.first, to_insert), split_res.second);
      root->template attach_left<false>(right_merge_res);

      is_changed = true;
      node_cnt++;
      return left_iterator(to_insert);
    }
    else 
      return end_left();
  }

  void recalc() const
  {
    left_pair.pointer = Node::template minimum<true>(root.get());
    right_pair.pointer = Node::template minimum<false>(root.get());
    is_changed = false;
  }

  template <bool is_left>
  std::pair<node_ptr, node_ptr> split(node_ptr t, const val_type<is_left>& key, const cmp_type<is_left>& cmp)
  {
    if (!t) { return { nullptr, nullptr }; }

    if (cmp(get<is_left>(t), key))
    {
      auto split_res = split<is_left>(t->template go_right<is_left>(), key, cmp);
      t->template attach_right<is_left>(split_res.first);
      return { t, split_res.second };
    }
    else
    {
      auto split_res = split<is_left>(t->template go_left<is_left>(), key, cmp);
      t->template attach_left<is_left>(split_res.second);
      return { split_res.first, t };
    }
  }
  std::pair<node_ptr, node_ptr> split_left(node_ptr t, const left_t& key)
  {
    return split<true>(t, key, static_cast<CompareLeft>(left_pair));
  }
  std::pair<node_ptr, node_ptr> split_right(node_ptr t, const right_t& key)
  {
    return split<false>(t, key, static_cast<CompareRight>(right_pair));
  }

  template <bool is_left>
  node_ptr merge(node_ptr t1, node_ptr t2, const cmp_type<is_left>& prim_cmp, const cmp_type<!is_left>& sec_cmp)
  {
    if (!t2) { return t1; }
    if (!t1) { return t2; }
    if (prim_cmp(get<is_left>(t2), get<is_left>(t1)))
    {
      std::swap(t1, t2);
    }
    if (sec_cmp(get<!is_left>(t1), get<!is_left>(t2)))
    {
      t1->template attach_right<is_left>(merge<is_left>(t1->template go_right<is_left>(), t2, prim_cmp, sec_cmp));
      return t1;
    }
    else
    {
      t2->template attach_left<is_left>(merge<is_left>(t1, t2->template go_left<is_left>(), prim_cmp, sec_cmp));
      return t2;
    }
  }
  node_ptr merge_left(node_ptr t1, node_ptr t2)
  {
    return merge<true>(t1, t2, static_cast<CompareLeft>(left_pair), static_cast<CompareRight>(right_pair));
  }
  node_ptr merge_right(node_ptr t1, node_ptr t2)
  {
    return merge<false>(t1, t2, static_cast<CompareRight>(right_pair), static_cast<CompareLeft>(left_pair));
  }

  template <bool is_left>
  val_type<!is_left> const& at(val_type<is_left> const& key) const
  {
    node_ptr temp_it;
    if constexpr (is_left)
      temp_it = find_node_left(key);
    else
      temp_it = find_node_right(key);
    
    if (!temp_it)
    {
      throw std::out_of_range("element with given key doesn't exist");
    }
    return get<!is_left>(temp_it);
  }

  template <bool is_left> bool erase(val_type<is_left> const& key)
  {
    iterator<is_left> it(nullptr);
    bool not_exist;
    if constexpr (is_left)
    {
      it = find_left(key);
      not_exist = (it == end_left());
    }
    else
    {
      it = find_right(key);
      not_exist = (it == end_right());
    }

    if (not_exist) 
      return false;
    else
    {
      if constexpr (is_left)
        erase_left(it);
      else 
        erase_right(it);

      return true;
    }
  }

  template <bool is_left>
  iterator<is_left> erase(iterator<is_left> first, iterator<is_left> last)
  {
    while (first != last)
    {
      if constexpr (is_left)
        erase_left(first++);
      else 
        erase_right(first++); 
    }
    return last;
  }

  template <bool is_left>
  iterator<is_left> lower_bound(const val_type<is_left>& key, const cmp_type<is_left>& cmp) const
  {
    node_ptr curr = root->template go_left<is_left>();
    node_ptr res = nullptr;
    while (curr)
    {
      if (cmp(get<is_left>(curr), key))
      {
        curr = curr->template go_right<is_left>();
      }
      else
      {
        res = curr;
        curr = curr->template go_left<is_left>();
      }
    }

    if (res) 
      return iterator<is_left>(res); 
    else if constexpr (is_left)
      return end_left(); 
    else 
      return end_right();
  }

  template <bool is_left>
  iterator<is_left> upper_bound(const val_type<is_left>& key, const cmp_type<is_left>& cmp) const
  {
    node_ptr curr = root->template go_left<is_left>();
    node_ptr res = nullptr;
    while (curr)
    {
      if (cmp(key, get<is_left>(curr)))
      {
        res = curr;
        curr = curr->template go_left<is_left>();
      }
      else
      {
        curr = curr->template go_right<is_left>();
      }
    }

    if (res) 
      return iterator<is_left>(res);
    else if constexpr (is_left) 
      return end_left();
    else
      return end_right();
    
  }

  template <bool is_left>
  const val_type<!is_left>& at_or_default(const val_type<is_left>& key)
  {
    iterator<is_left> temp_prim_it(nullptr), primary_end_it(nullptr);
    iterator<!is_left> temp_sec_it(nullptr), secondary_end_it(nullptr);
    if constexpr (is_left)
    {
      temp_prim_it = find_left(key);
      temp_sec_it = find_right(right_t());
      primary_end_it = end_left();
      secondary_end_it = end_right();
    }
    else
    {
      temp_prim_it = find_right(key);
      temp_sec_it = find_left(left_t());
      primary_end_it = end_right();
      secondary_end_it = end_left();
    }

    if (temp_prim_it == primary_end_it)
    {
      if (temp_sec_it != secondary_end_it)
      {
        if constexpr (is_left)
          erase_right(temp_sec_it);
        else
          erase_left(temp_sec_it);
      }

      if constexpr (is_left)
        return *(insert(key, right_t()).flip());
      else
        return *insert(left_t(), key);
    }
    else
      return *(temp_prim_it.flip());
  }
};

template <typename Left, typename Right, typename CompareLeft,
  typename CompareRight>
  class bimap<Left, Right, CompareLeft, CompareRight>::Node
{
  friend class bimap;
  class Tree_node;
  using node_ptr = Node *;
  using tree_node_ptr = Tree_node *;
  using left_t = Left;
  using right_t = Right;

  Tree_node left_node;
  Tree_node right_node;

  static node_ptr safe_ret_owner(tree_node_ptr node)
  {
    return node ? node->owner : nullptr;
  }

public:
  Node() 
    : left_node(this)
    , right_node(this) 
  {}

  virtual ~Node() {}

  template <bool is_left> tree_node_ptr get_tree_node()
  {
    if constexpr (is_left)
      return &left_node;
    else
      return &right_node;
  }

  template <bool is_left> node_ptr parent()
  {
    tree_node_ptr p = get_tree_node<is_left>()->go_parent();
    return safe_ret_owner(p);
  }

  template <bool is_left> node_ptr go_right()
  {
    tree_node_ptr p = get_tree_node<is_left>()->go_right();
    return safe_ret_owner(p);
  }

  template <bool is_left> node_ptr go_left()
  {
    tree_node_ptr p = get_tree_node<is_left>()->go_left();
    return safe_ret_owner(p);
  }

  template <bool is_left> void attach_left(node_ptr node)
  {
    tree_node_ptr to_attach = node ? node->template get_tree_node<is_left>() : nullptr;
    get_tree_node<is_left>()->attach_left(to_attach);
  }
  template <bool is_left> void attach_right(node_ptr node)
  {
    tree_node_ptr to_attach = node ? node->template get_tree_node<is_left>() : nullptr;
    get_tree_node<is_left>()->attach_right(to_attach);
  }

  void nullify()
  {
    left_node.left = nullptr;
    left_node.right = nullptr;
    left_node.parent = nullptr;
    right_node.left = nullptr;
    right_node.right = nullptr;
    right_node.parent = nullptr;
  }

  template <bool is_left> static node_ptr next(node_ptr curr)
  {
    if (!curr) { return nullptr; }

    tree_node_ptr temp = Tree_node::next(curr->template get_tree_node<is_left>());
    return safe_ret_owner(temp);
  }

  template <bool is_left> static node_ptr prev(node_ptr curr)
  {
    if (!curr) { return nullptr; }

    tree_node_ptr temp = Tree_node::prev(curr->template get_tree_node<is_left>());
    return safe_ret_owner(temp);
  }

  template <bool is_left> static node_ptr minimum(node_ptr curr)
  {
    return Tree_node::minimum(curr->template get_tree_node<is_left>())->owner;
  }

  template <bool is_left> static node_ptr maximum(node_ptr curr)
  {
    return Tree_node::maximum(curr->template get_tree_node<is_left>())->owner;
  }
};

template <typename Left, typename Right, typename CompareLeft,
  typename CompareRight>
  class bimap<Left, Right, CompareLeft, CompareRight>::Val_node
  : public bimap<Left, Right, CompareLeft, CompareRight>::Node
{
  friend class bimap;
  using left_t = Left;
  using right_t = Right;

  left_t left_val;
  right_t right_val;

public:
  Val_node(const left_t& l_value, const right_t& r_value)
    : Node(), left_val(l_value), right_val(r_value)
  {}
  Val_node(left_t&& l_value, const right_t& r_value)
    : Node(), left_val(std::move(l_value)), right_val(r_value)
  {}
  Val_node(const left_t& l_value, right_t&& r_value)
    : Node(), left_val(l_value), right_val(std::move(r_value))
  {}
  Val_node(left_t&& l_value, right_t&& r_value)
    : Node(), left_val(std::move(l_value)), right_val(std::move(r_value))
  {}

  ~Val_node() {}
};

template <typename Left, typename Right, typename CompareLeft,
  typename CompareRight>
  class bimap<Left, Right, CompareLeft, CompareRight>::Node::Tree_node
{
  friend Node;
  using node_ptr = Node *;
  using tree_node_ptr = Tree_node *;
  tree_node_ptr left, right, parent;
  node_ptr owner;

  tree_node_ptr go_parent() { return parent; }
  tree_node_ptr go_right() { return right; }
  tree_node_ptr go_left() { return left; }

  static tree_node_ptr maximum(tree_node_ptr curr)
  {
    while (curr->go_right())
    {
      curr = curr->go_right();
    }
    return curr;
  }
  static tree_node_ptr minimum(tree_node_ptr curr)
  {
    while (curr->go_left())
    {
      curr = curr->go_left();
    }
    return curr;
  }

  static tree_node_ptr next(tree_node_ptr curr)
  {
    if (curr)
    {
      if (curr->go_right())
      {
        return minimum(curr->go_right());
      }
      tree_node_ptr par = curr->go_parent();
      while (par && curr == par->go_right())
      {
        curr = par;
        par = par->go_parent();
      }
      return par;
    }
    return nullptr;
  }
  static tree_node_ptr prev(tree_node_ptr curr)
  {
    if (curr)
    {
      if (curr->go_left())
      {
        return maximum(curr->go_left());
      }
      tree_node_ptr par = curr->go_parent();
      while (par && curr == par->go_left())
      {
        curr = par;
        par = par->go_parent();
      }
      return par;
    }
    return nullptr;
  }

  // Прикрепляет левого сына
  void attach_left(tree_node_ptr node)
  {
    left = node;
    if (left) { left->parent = this; }
  }
  // Прикрепляет правого сына
  void attach_right(tree_node_ptr node)
  {
    right = node;
    if (right) { right->parent = this; }
  }

  void nullify()
  {
    left = nullptr;
    right = nullptr;
  }

public:
  Tree_node(node_ptr owner)
    : left(nullptr)
    , right(nullptr)
    , parent(nullptr)
    , owner(owner)
  {}
};
