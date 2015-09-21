/**
 * Serverpp Collection
 *
 * Description: Defines and implements data structures.
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#ifndef __COLLECTION_SPP_H__
#define __COLLECTION_SPP_H__

#include <string>
#include <list>

namespace spp
{
    /**
     * Suffix Tree: A string map with O(L) access where L is the length
     * of the inserted string.
     */
    template <class T>
    class SuffixTree
    {
    public:
        // Constructor
        SuffixTree(void)
        {
            m_nodes = new Node();
            m_nodes->value = NULL;
            m_nodes->index = 0;
        };

        // Destructor
        ~SuffixTree(void)
        {
            free_nodes(m_nodes);
        }

    private:
        // Internal tree node.
        struct Node
        {
        public:
            std::list<Node*> chars;
            char index;
            T value;
        };

        // Internal node collection.
        Node* m_nodes;

        void free_nodes(Node* node)
        {
            typename std::list<Node*>::iterator it;

            if (node->chars.size() == 0)
            {
                delete node;
                return;
            }

            for (it = node->chars.begin(); it != node->chars.end(); it++)
                free_nodes(*it);

            delete node;
        }

    public:
        // Suffix Tree setter.
        void set(std::string key, T value)
        {
            typename std::list<Node*>::iterator it;
            Node *next, *temp, *index;
            unsigned int i;
            char c;

            temp = m_nodes;

            // Process key character by character.
            for (i = 0; i < key.length(); i++)
            {
                c = key[i];
                index = NULL;

                // Search through characters for a matching node.
                for (it = temp->chars.begin(); it != temp->chars.end(); it++)
                {
                    next = *it;
                    if (next->index == c)
                    {
                        index = next;
                        break;
                    }
                }

                // Create a new node if not found and add it to the list.
                if (index == NULL)
                {
                    index = new Node();
                    index->index = c;
                    index->value = NULL;
                    temp->chars.push_back(index);
                }

                temp = index;
            }

            temp->value = value;
        }

        // Suffix Tree getter.
        T get(std::string key)
        {
            typename std::list<Node*>::iterator it;
            Node *temp, *index;
            unsigned int i;
            char c;

            temp = m_nodes;

            // Search for the provided key.
            for (i = 0; i < key.length(); i++)
            {
                c = key[i];
                index = NULL;

                // Get the next node.
                for (it = temp->chars.begin(); it != temp->chars.end(); it++)
                {
                    if ((*it)->index == c)
                    {
                        index = *it;
                        break;
                    }
                }

                // If the node is not found, return NULL
                if (index == NULL)
                    return NULL;

                temp = index;
            }

            return temp->value;
        }
    };
}

#endif