#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <memory>
#include <utility>

namespace MeshCraft::Editor {

// A document has to be deeply copied before it is frozen here. After that,
// undo and review history may safely share one immutable object graph; every
// restore path still creates a writable deep copy before editing resumes.
using DocumentSnapshot = std::shared_ptr<const Mc3::Mc3Document>;

inline DocumentSnapshot freezeDocument(Mc3::Mc3Document independentDocument)
{
    return std::make_shared<const Mc3::Mc3Document>(std::move(independentDocument));
}

} // namespace MeshCraft::Editor
