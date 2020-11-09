#pragma once

#include <queue>

#include <b1/session/session.hpp>
#include <eosio/chain/exceptions.hpp>

namespace eosio::session {

/// \brief Represents a container of pending sessions to be committed.
template <typename Session>
class undo_stack {
 public:
   using parent_type        = Session;
   using session_type       = session<Session>;
   using element_type       = std::variant<Session*, session_type*>;
   using const_element_type = std::variant<const Session*, const session_type*>;

   /// \brief Constructor.
   /// \param head The session that the changes are merged into when commit is called.
   undo_stack(Session& head);
   undo_stack(const undo_stack&) = delete;
   undo_stack(undo_stack&&)      = default;
   ~undo_stack();

   undo_stack& operator=(const undo_stack&) = delete;
   undo_stack& operator=(undo_stack&&) = default;

   /// \brief Adds a new session to the top of the stack.
   void push();

   /// \brief Merges the changes of the top session into the session below it in the stack.
   void squash();

   /// \brief Pops the top session off the stack and discards the changes in that session.
   void undo();

   /// \brief Commits the sessions at the bottom of the stack up to and including the provided revision.
   /// \param revision The revision number to commit up to.
   /// \remarks Each time a session is push onto the stack, a revision is assigned to it.
   void commit(int64_t revision);

   bool   empty() const;
   size_t size() const;

   /// \brief The starting revision number of the stack.
   int64_t revision() const;

   /// \brief Sets the starting revision number of the stack.
   /// \remarks This can only be set when the stack is empty and it can't be set
   /// to value lower than the current revision.
   void revision(int64_t revision);

   /// \brief Returns the head session (the session at the top of the stack).
   element_type top();

   /// \brief Returns the head session (the session at the top of the stack).
   const_element_type top() const;

   /// \brief Returns the session at the bottom of the stack.
   /// \remarks This is the next session to be committed.
   element_type bottom();

   /// \brief Returns the session at the bottom of the stack.
   /// \remarks This is the next session to be committed.
   const_element_type bottom() const;

 private:
   int64_t                  m_revision{ 0 };
   Session*                 m_head;
   std::deque<session_type> m_sessions; // Need a deque so pointers don't become invalidated.  The session holds a
                                        // pointer to the parent internally.
};

template <typename Session>
undo_stack<Session>::undo_stack(Session& head) : m_head{ &head } {}

template <typename Session>
undo_stack<Session>::~undo_stack() {
   for (auto& session : m_sessions) { session.undo(); }
}

template <typename Session>
void undo_stack<Session>::push() {
   if (m_sessions.empty()) {
      m_sessions.emplace_back(*m_head);
   } else {
      m_sessions.emplace_back(m_sessions.back());
   }
   ++m_revision;
}

template <typename Session>
void undo_stack<Session>::squash() {
   if (m_sessions.empty()) {
      return;
   }
   m_sessions.back().commit();
   m_sessions.back().detach();
   m_sessions.pop_back();
   --m_revision;
}

template <typename Session>
void undo_stack<Session>::undo() {
   if (m_sessions.empty()) {
      return;
   }
   m_sessions.back().detach();
   m_sessions.pop_back();
   --m_revision;
}

template <typename Session>
void undo_stack<Session>::commit(int64_t revision) {
   if (m_sessions.empty()) {
      return;
   }

   revision              = std::min(revision, m_revision);
   auto initial_revision = static_cast<int64_t>(m_revision - m_sessions.size() + 1);
   if (initial_revision > revision) {
      return;
   }

   auto start_index = revision - initial_revision + 1;

   for (int64_t i = start_index; i >= 0; --i) { m_sessions[i].commit(); }
   m_sessions.erase(std::begin(m_sessions), std::begin(m_sessions) + start_index);
   if (!m_sessions.empty()) {
      m_sessions.front().attach(*m_head);
   }
}

template <typename Session>
bool undo_stack<Session>::empty() const {
   return m_sessions.empty();
}

template <typename Session>
size_t undo_stack<Session>::size() const {
   return m_sessions.size();
}

template <typename Session>
int64_t undo_stack<Session>::revision() const {
   return m_revision;
}

template <typename Session>
void undo_stack<Session>::revision(int64_t revision) {
   if (!empty()) {
      return;
   }

   if (revision <= m_revision) {
      return;
   }

   m_revision = revision;
}

template <typename Session>
typename undo_stack<Session>::element_type undo_stack<Session>::top() {
   if (!m_sessions.empty()) {
      return &m_sessions.back();
   }
   return m_head;
}

template <typename Session>
typename undo_stack<Session>::const_element_type undo_stack<Session>::top() const {
   if (!m_sessions.empty()) {
      return &m_sessions.back();
   }
   return m_head;
}

template <typename Session>
typename undo_stack<Session>::element_type undo_stack<Session>::bottom() {
   if (!m_sessions.empty()) {
      return &m_sessions.front();
   }
   return m_head;
}

template <typename Session>
typename undo_stack<Session>::const_element_type undo_stack<Session>::bottom() const {
   if (!m_sessions.empty()) {
      return &m_sessions.front();
   }
   return m_head;
}

} // namespace eosio::session