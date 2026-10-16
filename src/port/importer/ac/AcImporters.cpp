#include "AcImporters.h"

#include "AcResourceType.h"
#include "EvwAnimeFactory.h"
#include "NpcAnimationFactory.h"
#include "port/ShipCompat.h"

#include <ship/resource/File.h>
#include <ship/resource/ResourceLoader.h>
#include <ship/resource/ResourceManager.h>

extern "C" void AC_RegisterResourceImporters(void) {
    auto loader = ShipCompat::GetResourceManager()->GetResourceLoader();

    loader->RegisterResourceFactory(std::make_shared<AC::ResourceFactoryBinaryEvwAnimeV0>(), RESOURCE_FORMAT_BINARY,
                                    "TextureAnimation", static_cast<uint32_t>(AC::AcResourceType::TextureAnimation),
                                    0);
    loader->RegisterResourceFactory(std::make_shared<AC::ResourceFactoryBinaryNpcAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "NpcAnimation",
                                    static_cast<uint32_t>(AC::AcResourceType::NpcAnimation), 0);
}
