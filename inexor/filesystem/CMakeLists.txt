
# We work in 2 different scopes in this file: One is this subproject added through add_lib and called module_<ourname>
#                                             The other one is the scope of config_<ourname> used to bind this module
#                                             into other modules and hence operates **in the scope of where it gets executed!**

declare_module(filesystem .) # Get the source lists of all files in this folder (and register us as module)
add_lib(filesystem)          # Our module is **a static lib** which gets linked into other modules or apps.
                             # Its name is "module_filesystem" (due to this function)

# Acquire our dependencies for this module
# (NOLINK is used since we're only a module/static library, no app, we do not link anything.)
# ("module_filesystem" is used and not just "filesystem", since we require it for this subproject)
config_sdl(module_filesystem              NOLINK)
config_boost_filesystem(module_filesystem NOLINK)
config_rapidjson(module_filesystem        NOLINK)
config_util(module_filesystem             NOLINK)

# This function is used to bind this module into another module/application
function(config_filesystem targ)
  message(STATUS "Configuring ${targ} with filesystem")

  if (";${ARGN};" MATCHES ";NOLINK;") # Some argument passing, just copy that.
    set(NOLINK "NOLINK")
  endif()

  target_link_libs(${targ} module_filesystem ${NOLINK}) # Tell the requiring module that it needs to link with our static lib

  # Also tell the other module that it needs our dependencies:
  config_sdl(${targ} ${NOLINK}) # we need sdl since we havent refractored cube.hpp good enough yet
  config_boost_filesystem(${targ} ${NOLINK})
  config_rapidjson(${targ} ${NOLINK})
  config_util(${targ} ${NOLINK})

endfunction()