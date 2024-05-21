#pragma once

#if !defined(__cplusplus)
static_assert(false && "list.h is c++ only");
#endif

#define NULL_NODE (list_node<T> T::*)nullptr != NODE

namespace chs
{
    //////////////////////////////////////////////////////////////////////
    // base list node class, 2 pointers

    template <typename T> struct list_node_base
    {
        T *next;
        T *prev;
    };

    //////////////////////////////////////////////////////////////////////
    // base node is wrapped so we can get the offset to it

    template <typename T> struct list_node
    {
        list_node_base<T> node;
    };

    //////////////////////////////////////////////////////////////////////
    // template base

    template <typename T, list_node<T> T::*NODE, bool is_member> class list_base
    {
    };

    //////////////////////////////////////////////////////////////////////
    // specialization for instances using list_node as member field

    template <typename T, list_node<T> T::*NODE> class list_base<T, NODE, true> : protected list_node<T>
    {
    protected:
        static size_t offset()
        {
            list_node<T> *b = &(((T *)0)->*NODE);
            return size_t(&b->node);
        }

        typedef list_base<T, NODE, true> list_type;
    };

    //////////////////////////////////////////////////////////////////////
    // specialization for instances deriving from list_node

    template <typename T, list_node<T> T::*NODE> class list_base<T, NODE, false> : protected list_node<T>
    {
        // static_assert(!std::is_polymorphic<T>::value, "polymorphic! use the member-node version");

    protected:
        static size_t offset()
        {
            list_node_base<T> T::*n = static_cast<list_node_base<T> T::*>(&T::node);
            return (size_t)(&(((T *)0)->*n));
        }
        typedef list_base<T, NODE, false> list_type;
    };

    //////////////////////////////////////////////////////////////////////

    template <typename T, list_node<T> T::*NODE = nullptr> class linked_list : protected list_base<T, NODE, NULL_NODE>
    {
    public:
        //////////////////////////////////////////////////////////////////////

        using list_base<T, NODE, NULL_NODE>::offset;
        using list_base<T, NODE, NULL_NODE>::node;

        //////////////////////////////////////////////////////////////////////

        typedef linked_list<T, NODE> list_t;

        typedef T *ptr;
        typedef T *pointer;
        typedef T const *const_ptr;
        typedef T const *const_pointer;
        typedef T &ref;
        typedef T &reference;
        typedef T const &const_ref;
        typedef T const &const_reference;

        //////////////////////////////////////////////////////////////////////

        typedef list_node_base<T> node_t;
        typedef node_t *node_ptr;
        typedef node_t &node_ref;
        typedef list_node_base<T> const const_node_t;
        typedef const_node_t *const_node_ptr;
        typedef const_node_t &const_node_ref;

    private:
        //////////////////////////////////////////////////////////////////////

        static list_t &transfer(list_t &a, list_t &b)
        {
            if(!a.empty()) {
                T *ot = a.tail();
                T *oh = a.head();
                T *rt = b.root();
                get_node(ot).next = rt;
                get_node(oh).prev = rt;
                get_node(rt).prev = ot;
                get_node(rt).next = oh;
                a.clear();
            } else {
                b.clear();
            }
            return b;
        }

        //////////////////////////////////////////////////////////////////////

        static ptr get_object(node_ptr node)
        {
            return reinterpret_cast<ptr>(reinterpret_cast<char *>(node) - offset());
        }

        //////////////////////////////////////////////////////////////////////

        static node_ref get_node(ptr obj)
        {
            return *reinterpret_cast<node_ptr>(reinterpret_cast<char *>(obj) + offset());
        }

        //////////////////////////////////////////////////////////////////////

        static const_node_ref get_node(const_ptr obj)
        {
            return *reinterpret_cast<const_node_ptr>(reinterpret_cast<char const *>(obj) + offset());
        }

        //////////////////////////////////////////////////////////////////////

        ptr root()
        {
            return reinterpret_cast<ptr>(reinterpret_cast<char *>(&node) - offset());
        }

        //////////////////////////////////////////////////////////////////////

        const_ptr const_root() const
        {
            return reinterpret_cast<const_ptr>(reinterpret_cast<char const *>(&node) - offset());
        }

        //////////////////////////////////////////////////////////////////////

        static ptr get_next(ptr const o)
        {
            return get_node(o).next;
        }

        //////////////////////////////////////////////////////////////////////

        static ptr get_prev(ptr const o)
        {
            return get_node(o).prev;
        }

        //////////////////////////////////////////////////////////////////////

        static void set_next(ptr o, ptr const p)
        {
            get_node(o).next = p;
        }

        //////////////////////////////////////////////////////////////////////

        static void set_prev(ptr o, ptr const p)
        {
            get_node(o).prev = p;
        }

    public:
        //////////////////////////////////////////////////////////////////////

        void clear()
        {
            node.next = root();
            node.prev = root();
        }

        //////////////////////////////////////////////////////////////////////

        void insert_before(ptr obj_before, ptr obj)
        {
            ptr &p = get_node(obj_before).prev;
            node_ref n = get_node(obj);
            get_node(p).next = obj;
            n.prev = p;
            p = obj;
            n.next = obj_before;
        }

        //////////////////////////////////////////////////////////////////////

        void insert_after(ptr obj_after, ptr obj)
        {
            ptr &n = get_node(obj_after).next;
            node_ref p = get_node(obj);
            get_node(n).prev = obj;
            p.next = n;
            n = obj;
            p.prev = obj_after;
        }

        //////////////////////////////////////////////////////////////////////

        void remove_range(ptr f, ptr l)
        {
            ptr p = prev(f);
            ptr n = next(l);
            set_next(p, n);
            set_prev(n, p);
        }

        //////////////////////////////////////////////////////////////////////

        ptr remove(ptr obj)
        {
            remove_range(obj, obj);
            return obj;
        }

        //////////////////////////////////////////////////////////////////////

        void move_range_before(ptr where, list_t &other, ptr f, ptr l)
        {
            other.remove_range(f, l);
            ptr p = get_prev(where);
            set_next(p, f);
            set_prev(f, p);
            set_prev(where, l);
            set_next(l, where);
        }

        //////////////////////////////////////////////////////////////////////

        void move_range_after(ptr where, list_t &other, ptr f, ptr l)
        {
            other.remove_range(f, l);
            ptr p = get_next(where);
            set_prev(p, l);
            set_next(l, p);
            set_next(where, f);
            set_prev(f, where);
        }

        //////////////////////////////////////////////////////////////////////

        linked_list()
        {
            clear();
        }

        //////////////////////////////////////////////////////////////////////

        explicit linked_list(list_t &other)
        {
            transfer(other, *this);
        }

        //////////////////////////////////////////////////////////////////////

        list_t &operator=(list_t &o)
        {
            return transfer(o, *this);
        }

        //////////////////////////////////////////////////////////////////////

        list_t const &operator=(list_t const &&o) = delete;

        //////////////////////////////////////////////////////////////////////

        list_t &operator=(list_t &&o) = delete;

        //////////////////////////////////////////////////////////////////////

        bool empty() const
        {
            return node.next == const_root();
        }

        //////////////////////////////////////////////////////////////////////

        size_t size() const
        {
            size_t count = 0;
            for(auto const &i : *this) {
                ++count;
            }
            return count;
        }

        //////////////////////////////////////////////////////////////////////

        ptr head()
        {
            return node.next;
        }
        ptr tail()
        {
            return node.prev;
        }

        const_ptr c_head() const
        {
            return node.next;
        }
        const_ptr c_tail() const
        {
            return node.prev;
        }

        ptr next(ptr obj)
        {
            return get_node(obj).next;
        }
        ptr prev(ptr obj)
        {
            return get_node(obj).prev;
        }

        const_ptr c_next(const_ptr obj) const
        {
            return get_node(obj).next;
        }
        const_ptr c_prev(const_ptr obj) const
        {
            return get_node(obj).prev;
        }

        ptr done()
        {
            return root();
        }
        const_ptr c_done() const
        {
            return const_root();
        }

        //////////////////////////////////////////////////////////////////////

        ptr remove(ref obj)
        {
            return remove(&obj);
        }

        void push_back(ref obj)
        {
            insert_before(*root(), obj);
        }
        void push_back(ptr obj)
        {
            insert_before(root(), obj);
        }

        void push_front(ref obj)
        {
            insert_after(*root(), obj);
        }
        void push_front(ptr obj)
        {
            insert_after(root(), obj);
        }

        ptr pop_front()
        {
            return empty() ? nullptr : remove(head());
        }
        ptr pop_back()
        {
            return empty() ? nullptr : remove(tail());
        }

        void insert_before(ref bef, ref obj)
        {
            insert_before(&bef, &obj);
        }
        void insert_after(ref aft, ref obj)
        {
            insert_after(&aft, &obj);
        }

        void remove_range(ref f, ref l)
        {
            remove_range(&f, &l);
        }

        void move_range_before(ref where, list_t &other, ref f, ref l)
        {
            move_range_before(&where, other, &f, &l);
        }

        void move_range_after(ref where, list_t &other, ref f, ref l)
        {
            move_range_after(&where, other, &f, &l);
        }

        //////////////////////////////////////////////////////////////////////

        void append(list_t &other_list)
        {
            if(!other_list.empty()) {
                ptr oh = other_list.head();
                ptr ot = other_list.tail();
                ptr rt = root();
                ptr mt = tail();
                set_next(mt, oh);
                set_prev(oh, mt);
                set_next(ot, rt);
                set_prev(rt, ot);
                other_list.clear();
            }
        }

        //////////////////////////////////////////////////////////////////////

        void prepend(list_t &other_list)
        {
            if(!other_list.empty()) {
                ptr oh = other_list.head();
                ptr ot = other_list.tail();
                ptr rt = root();
                ptr mh = head();
                set_prev(mh, ot);
                set_next(ot, mh);
                set_prev(oh, rt);
                set_next(rt, oh);
                other_list.clear();
            }
        }

        //////////////////////////////////////////////////////////////////////

        template <typename N> void copy_into(N &n)
        {
            static_assert(!std::is_same<N, list_t>::value, "can't copy to the same type!");
            for(auto &p : *this) {
                n.push_back(p);
            }
        }

        //////////////////////////////////////////////////////////////////////

        void split(ptr obj, list_t &new_list)
        {
            T *prev_obj = prev(obj);
            if(prev_obj == root()) {
                transfer(*this, new_list);
            } else {
                T *new_root = new_list.root();
                T *old_tail = tail();
                T *next_obj = next(obj);
                if(next_obj == root()) {
                    get_node(old_tail).prev = new_root;
                }
                set_next(old_tail, new_root);
                set_next(prev_obj, root());
                set_prev(root(), prev_obj);
                new_list.set_next(new_root, obj);
                new_list.set_prev(new_root, old_tail);
            }
        }

        //////////////////////////////////////////////////////////////////////
    private:
        static void merge(list_t &left, list_t &right)
        {
            ptr insert_point = right.root();
            ptr run_head = left.head();
            ptr ad = left.done();
            ptr bd = right.done();
            while(run_head != ad) {
                // find where to put a run
                do {
                    insert_point = get_next(insert_point);
                } while(insert_point != bd && *insert_point < *run_head);

                // scanned off the end?
                if(insert_point != bd) {
                    // no, find how long the run should be
                    ptr run_start = run_head;
                    ptr run_end;
                    do {
                        run_end = run_head;
                        run_head = get_next(run_head);
                    } while(run_head != ad && *run_head < *insert_point);

                    // insert it
                    ptr p = get_prev(insert_point);
                    set_prev(run_start, p);
                    set_next(p, run_start);
                    set_prev(insert_point, run_end);
                    set_next(run_end, insert_point);
                } else {    // yes, append remainder
                    ptr ot = left.tail();
                    ptr rt = right.root();
                    ptr mt = right.tail();
                    set_prev(run_head, mt);
                    set_next(mt, run_head);
                    set_prev(rt, ot);
                    set_next(ot, rt);
                    break;
                }
            }
        }

        //////////////////////////////////////////////////////////////////////
        // thanks to the putty guy

        static void merge_sort(list_t &list, size_t size)
        {
            if(size > 2) {
                // scan for midpoint
                size_t left_size = size / 2;
                size_t right_size = size - left_size;
                ptr pm = list.head();
                for(size_t s = 0; s < left_size; ++s) {
                    pm = list.next(pm);
                }

                // split into left, right
                list_t left;
                list_t right;
                ptr lr = left.root();
                ptr rr = right.root();
                ptr ot = list.tail();
                ptr oh = list.head();
                ptr pp = list.get_prev(pm);
                set_prev(lr, pp);
                set_next(lr, oh);
                set_prev(oh, lr);
                set_next(pp, lr);
                set_prev(rr, ot);
                set_next(rr, pm);
                set_prev(pm, rr);
                set_next(ot, rr);

                // sort them
                merge_sort(left, left_size);
                merge_sort(right, right_size);

                // stitch them back together
                merge(right, left);

                // move result back into list
                ot = left.tail();
                oh = left.head();
                lr = list.root();
                set_prev(oh, lr);
                set_next(ot, lr);
                set_prev(lr, ot);
                set_next(lr, oh);
            } else if(size > 1) {
                // dinky list of 2 entries, just fix it
                ptr h = list.head();
                ptr t = list.tail();
                if(*t < *h) {
                    ptr r = list.root();
                    set_next(r, t);
                    set_prev(r, h);
                    set_next(h, r);
                    set_prev(h, t);
                    set_next(t, h);
                    set_prev(t, r);
                }
            }
        }

        //////////////////////////////////////////////////////////////////////
    public:
        void sort()
        {
            merge_sort(*this, size());
        }

        //////////////////////////////////////////////////////////////////////

        void merge_into(list_t &other)
        {
            merge(*this, other);
            clear();
        }

        //////////////////////////////////////////////////////////////////////

        void remove_all()
        {
            clear();
        }

        //////////////////////////////////////////////////////////////////////

        void delete_all()
        {
            while(!empty()) {
                ptr i = pop_front();
                delete i;
            }
        }
    };

    //////////////////////////////////////////////////////////////////////

}    // namespace chs