#pragma once

#include "memory/heap.hpp"

namespace cosmos::stl {
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

        T* push_back_alloc(const std::size_t additional_size = 0) {
            const auto node = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + additional_size, alignof(Node)));

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

        T* insert_after_alloc(Node* current, const std::size_t additional_size = 0) {
            const auto node = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + additional_size, alignof(Node)));

            node->next = current->next;
            current->next = node;

            if (tail == current) {
                tail = node;
            }

            return &node->item;
        }

        T* insert_after_alloc(Iterator& it, const std::size_t additional_size = 0) {
            return insert_after_alloc(it.node, additional_size);
        }

        void remove_free(Node* prev, Node* current) {
            if (prev != nullptr) {
                prev->next = current->next;
            }

            if (head == current) {
                head = current->next;
            }
            if (tail == current) {
                tail = prev;
            }

            memory::heap::free(current);
        }

        void remove_free(Iterator& it) {
            const auto next = it.node->next;
            remove_free(it.prev, it.node);
            it.node = next;
        }

        Iterator begin() const {
            return { nullptr, head };
        }

        static Iterator end() {
            return { nullptr, nullptr };
        }
    };
} // namespace cosmos::stl
