#----------------------------------*-CMake-*----------------------------------#
# Copyright 2020 UT-Battelle, LLC and other Celeritas Developers.
# See the top-level COPYRIGHT file for details.
# SPDX-License-Identifier: (Apache-2.0 OR MIT)
#-----------------------------------------------------------------------------#

if(CELERITAS_BUILD_TESTS)
  # Set environment variables from Geant4-exported configuration
  set(_geant_test_env)
  foreach(_ds IN LISTS Geant4_DATASETS)
    list(APPEND _geant_test_env
      "${Geant4_DATASET_${_ds}_ENVVAR}=${Geant4_DATASET_${_ds}_PATH}")
  endforeach()
endif()

if(CELERITAS_USE_Geant4 AND CELERITAS_USE_ROOT)
  # Exporter
  add_executable(geant-exporter geant-exporter/geant-exporter.cc
    geant-exporter/ActionInitialization.cc
    geant-exporter/DetectorConstruction.cc
    geant-exporter/PhysicsList.cc
    geant-exporter/PrimaryGeneratorAction.cc
    geant-exporter/ImportProcessConverter.cc
    # TODO: Move these to main celeritas::geant library for use in acceleritas?
    geant-exporter/GeantExceptionHandler.cc
    geant-exporter/GeantLoggerAdapter.cc
  )
  celeritas_target_link_libraries(geant-exporter
    Celeritas::Core
    Celeritas::ROOT
    ROOT::Core
    ROOT::Tree
    XercesC::XercesC
    ${Geant4_LIBRARIES}
  )

  # Reader
  add_executable(geant-exporter-cat geant-exporter/geant-exporter-cat.cc)
  celeritas_target_link_libraries(geant-exporter-cat
    Celeritas::Core
    Celeritas::ROOT
  )

  if(CELERITAS_BUILD_TESTS)
    set(_geant_test_inp
      "${CMAKE_CURRENT_SOURCE_DIR}/geant-exporter/data/four-steel-slabs.gdml"
    )
    add_test(NAME "app/geant-exporter"
      COMMAND "$<TARGET_FILE:geant-exporter>"
      "${_geant_test_inp}" "test-data.root"
    )
    set_tests_properties("app/geant-exporter" PROPERTIES
      ENVIRONMENT "${_geant_test_env}"
      REQUIRED_FILES "${_geant_test_inp}"
    )

    add_test(NAME "app/geant-exporter-cat"
      COMMAND "$<TARGET_FILE:geant-exporter-cat>"
        "test-data.root"
    )
    set_tests_properties("app/geant-exporter-cat" PROPERTIES
      DEPENDS "app/geant-exporter"
      REQUIRED_FILES "test-data.root"
    )
  endif()
endif()

#-----------------------------------------------------------------------------#
# DEMO: physics interactions
#-----------------------------------------------------------------------------#
if(CELERITAS_BUILD_DEMOS AND NOT CELERITAS_USE_JSON)
  message(SEND_ERROR "JSON support is required for demos")
endif()

if(CELERITAS_BUILD_DEMOS)
  add_library(celeritas_demo_interactor
    demo-interactor/LoadXs.cc
    demo-interactor/KNDemoIO.cc
    demo-interactor/XsGridParams.cc
  )
  celeritas_target_link_libraries(celeritas_demo_interactor PUBLIC
    celeritas
    nlohmann_json::nlohmann_json
  )

  if(CELERITAS_USE_CUDA)
    add_executable(demo-interactor
      demo-interactor/demo-interactor.cc
      demo-interactor/KNDemoRunner.cc
      demo-interactor/KNDemoKernel.cu
      demo-interactor/KNDemoKernel.thrust.cu
    )
    celeritas_target_link_libraries(demo-interactor celeritas_demo_interactor)

    if(CELERITAS_BUILD_TESTS)
      set(_driver "${CMAKE_CURRENT_SOURCE_DIR}/demo-interactor/simple-driver.py")
      add_test(NAME "app/demo-interactor"
        COMMAND "$<TARGET_FILE:Python::Interpreter>" "${_driver}"
      )
      set(_env
        "CELERITAS_DEMO_EXE=$<TARGET_FILE:demo-interactor>"
        "CELER_DISABLE_PARALLEL=1"
      )
      set_tests_properties("app/demo-interactor" PROPERTIES
        ENVIRONMENT "${_env}"
        RESOURCE_LOCK gpu
        REQUIRED_FILES "${_driver}"
      )
    endif()
  endif()

  # Build CPU version
  add_executable(host-demo-interactor
    demo-interactor/host-demo-interactor.cc
    demo-interactor/HostKNDemoRunner.cc
  )
  celeritas_target_link_libraries(host-demo-interactor
    celeritas
    celeritas_demo_interactor
  )

  if(CELERITAS_BUILD_TESTS)
    set(_driver "${CMAKE_CURRENT_SOURCE_DIR}/demo-interactor/simple-driver.py")
    add_test(NAME "app/host-demo-interactor"
      COMMAND "$<TARGET_FILE:Python::Interpreter>" "${_driver}"
    )
    set(_env
      "CELERITAS_DEMO_EXE=$<TARGET_FILE:host-demo-interactor>"
      "CELER_DISABLE_DEVICE=1"
      "CELER_DISABLE_PARALLEL=1"
    )
    set_tests_properties("app/host-demo-interactor" PROPERTIES
      ENVIRONMENT "${_env}"
      REQUIRED_FILES "${_driver}"
    )
  endif()
endif()

#-----------------------------------------------------------------------------#
# DEMO: geometry tracking
#-----------------------------------------------------------------------------#

if(CELERITAS_BUILD_DEMOS AND CELERITAS_USE_CUDA AND CELERITAS_USE_VecGeom)
  # Since the demo kernel links against VecGeom, which requires CUDA separable
  # compilation, it cannot be linked directly into an executable.
  celeritas_add_library(celeritas_demo_rasterizer
    demo-rasterizer/RDemoRunner.cc
    demo-rasterizer/RDemoKernel.cu
    demo-rasterizer/ImageIO.cc
    demo-rasterizer/ImageStore.cc
  )
  celeritas_target_link_libraries(celeritas_demo_rasterizer
    PRIVATE
      Celeritas::Core
      nlohmann_json::nlohmann_json
      VecGeom::vecgeom
  )

  # Add the executable
  add_executable(demo-rasterizer
    demo-rasterizer/demo-rasterizer.cc
  )
  celeritas_target_link_libraries(demo-rasterizer
    celeritas
    VecGeom::vecgeom
    celeritas_demo_rasterizer
    nlohmann_json::nlohmann_json
  )

  if(CELERITAS_BUILD_TESTS)
    set(_driver "${CMAKE_CURRENT_SOURCE_DIR}/demo-rasterizer/simple-driver.py")
    set(_gdml_inp "${PROJECT_SOURCE_DIR}/test/geometry/data/twoBoxes.gdml")
    add_test(NAME "app/demo-rasterizer"
      COMMAND "$<TARGET_FILE:Python::Interpreter>" "${_driver}" "${_gdml_inp}"
    )
    set(_env
      "CELERITAS_DEMO_EXE=$<TARGET_FILE:demo-rasterizer>"
      "CELER_DISABLE_PARALLEL=1"
    )
    set_tests_properties("app/demo-rasterizer" PROPERTIES
      ENVIRONMENT "${_env}"
      RESOURCE_LOCK gpu
      REQUIRED_FILES "${_driver};${_gdml_inp}"
    )
  endif()
endif()

#-----------------------------------------------------------------------------#
# DEMO: full physics loop
#-----------------------------------------------------------------------------#

if(CELERITAS_BUILD_DEMOS AND CELERITAS_USE_VecGeom)
  set(_cuda_src)
  if(CELERITAS_USE_CUDA)
    set(_cuda_src
      demo-loop/LDemoKernel.cu
    )
  endif()
  celeritas_add_library(celeritas_demo_loop
    demo-loop/LDemoIO.cc
    demo-loop/LDemoParams.cc
    demo-loop/LDemoRun.cc
    ${_cuda_src}
  )
  celeritas_target_link_libraries(celeritas_demo_loop
    PRIVATE
    Celeritas::Core
    nlohmann_json::nlohmann_json
    Celeritas::ROOT
  )

  if(CELERITAS_USE_CUDA)
    celeritas_target_link_libraries(celeritas_demo_loop
      PRIVATE
      VecGeom::vecgeom
    )
  endif()

  # Add the executable
  add_executable(demo-loop
    demo-loop/demo-loop.cc
  )
  # Note: for static linking the library order is relevant.
  celeritas_target_link_libraries(demo-loop
    celeritas_demo_loop
    celeritas
    VecGeom::vecgeom
    nlohmann_json::nlohmann_json
  )

  # TODO: update input files and enable test
  if(CELERITAS_BUILD_TESTS AND CELERITAS_USE_Geant4 AND CELERITAS_USE_ROOT)
    set(_driver "${CMAKE_CURRENT_SOURCE_DIR}/demo-loop/simple-driver.py")
    set(_gdml_inp "${PROJECT_SOURCE_DIR}/app/demo-loop/simple_cms.gdml")
    set(_hepmc3_inp "${PROJECT_SOURCE_DIR}/app/demo-loop/input.hepmc3")
    add_test(NAME "app/demo-loop"
      COMMAND "$<TARGET_FILE:Python::Interpreter>"
      "${_driver}" "${_gdml_inp}" "${_hepmc3_inp}"
    )
    set(_env
      "CELERITAS_DEMO_EXE=$<TARGET_FILE:demo-loop>"
      "CELERITAS_GEANT_EXPORTER_EXE=$<TARGET_FILE:geant-exporter>"
      "CELER_DISABLE_PARALLEL=1"
    )
    set_tests_properties("app/demo-loop" PROPERTIES
      ENVIRONMENT "${_env};${_geant_test_env}"
      RESOURCE_LOCK gpu
      REQUIRED_FILES "${_driver};${_gdml_inp};${_hepmc3_inp}"
      DISABLED true
    )
  endif()
endif()

#-----------------------------------------------------------------------------#
