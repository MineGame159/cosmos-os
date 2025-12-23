#pragma once

#include "mem.hpp"


#include <cstddef>

namespace stl {
    template <typename T>
    struct LinkedList {
        struct Node {
            Node* next;
            T item;
        };

        struct Iterator {
            Node* prev;
            Node* node;

            bool operator==(const Iterator& other) const {
                return node == other.node;
            }

            T* operator*() const {
                return &node->item;
            }

            T* operator->() const {
                return &node->item;
            }

            Iterator& operator++() {
                prev = node;
                node = node->next;
                return *this;
            }

            T* operator++(int) {
                const auto item = &node->item;
                prev = node;
                node = node->next;
                return item;
            }
        };

        Node* head = nullptr;
        Node* tail = nullptr;

        [[nodiscard]]
        bool empty() const {
            return head == nullptr;
        }

        [[nodiscard]]
        bool single_item() const {
            return head != nullptr && head == tail;
        }

        T* push_back_alloc(const size_t additional_size = 0) {
            const auto node = static_cast<Node*>(aligned_alloc(sizeof(Node) + additional_size, alignof(Node)));

            if (head == nullptr) {
                head = node;
                tail = node;
            } else {
                tail->next = node;
                tail = node;
            }

            node->next = nullptr;
            return &node->item;
        }

        T* insert_after_alloc(Node* current, const size_t additional_size = 0) {
            const auto node = static_cast<Node*>(aligned_alloc(sizeof(Node) + additional_size, alignof(Node)));

            node->next = current->next;
            current->next = node;

            if (tail == current) {
                tail = node;
            }

            return &node->item;
        }

        T* insert_after_alloc(Iterator& it, const size_t additional_size = 0) {
            return insert_after_alloc(it.node, additional_size);
        }

        void remove(Node* prev, Node* current) {
            if (prev != nullptr) {
                prev->next = current->next;
            }

            if (head == current) {
                head = current->next;
            }
            if (tail == current) {
                tail = prev;
            }

            current->next = nullptr;
        }

        Node* remove(Iterator& it) {
            const auto next = it.node->next;
            remove(it.prev, it.node);
            const auto node = it.node;
            it.node = next;
            return node;
        }

        bool remove(T* item) {
            for (auto it = begin(); it != end(); ++it) {
                if (*it == item) {
                    remove_free(it);
                    return true;
                }
            }

            return false;
        }

        void remove_free(Node* prev, Node* current) {
            remove(prev, current);
            free(current);
        }

        void remove_free(Iterator& it) {
            const auto next = it.node->next;
            remove_free(it.prev, it.node);
            it.node = next;
        }

        bool remove_free(T* item) {
            for (auto it = begin(); it != end(); ++it) {
                if (*it == item) {
                    remove_free(it);
                    return true;
                }
            }

            return false;
        }

        Iterator begin() const {
            return { nullptr, head };
        }

        static Iterator end() {
            return { nullptr, nullptr };
        }
    };
} // namespace stl
