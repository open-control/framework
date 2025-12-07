#include "BindingHandle.hpp"

#include "InputBinding.hpp"

namespace oc::core {

void BindingHandle::unbind() {
    if (isValid()) {
        registry_->removeById(id_);
        registry_ = nullptr;
        id_ = 0;
    }
}

}  // namespace oc::core
