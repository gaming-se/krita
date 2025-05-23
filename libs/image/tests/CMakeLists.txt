# cmake in some versions for some not yet known reasons fails to run automoc
# on random targets (changing target names already has an effect)
# As temporary workaround skipping build of tests on these versions for now
# See https://mail.kde.org/pipermail/kde-buildsystem/2015-June/010819.html
# extend range of affected cmake versions as needed
if(NOT ${CMAKE_VERSION} VERSION_LESS 3.1.3 AND
   NOT ${CMAKE_VERSION} VERSION_GREATER 3.2.3)
    message(WARNING "Skipping krita/image/tests, CMake in at least versions 3.1.3 - 3.2.3 seems to have a problem with automoc.  \n(FRIENDLY REMINDER: PLEASE DON'T BREAK THE TESTS!)")
    set (HAVE_FAILING_CMAKE TRUE)
else()
    set (HAVE_FAILING_CMAKE FALSE)
endif()

include(KritaAddBrokenUnitTest)

set(KisRandomGenerator2DDemoSources KisRandomGenerator2DDemo.cpp kimageframe.cpp)
ki18n_wrap_ui(KisRandomGenerator2DDemoSources KisRandomGenerator2DDemo.ui)
add_executable(KisRandomGenerator2DDemo ${KisRandomGenerator2DDemoSources})
target_link_libraries(KisRandomGenerator2DDemo kritaimage)
ecm_mark_as_test(KisRandomGenerator2DDemo)


kis_add_tests(
    kis_base_node_test.cpp
    kis_fast_math_test.cpp
    kis_node_test.cpp
    kis_node_facade_test.cpp
    kis_fixed_paint_device_test.cpp
    kis_layer_test.cpp
    kis_effect_mask_test.cpp
    kis_iterator_test.cpp
    kis_painter_test.cpp
    kis_count_visitor_test.cpp
    kis_projection_test.cpp
    kis_properties_configuration_test.cpp
    kis_transaction_test.cpp
    kis_pixel_selection_test.cpp
    kis_group_layer_test.cpp
    kis_paint_layer_test.cpp
    kis_adjustment_layer_test.cpp
    kis_annotation_test.cpp
    kis_convolution_painter_test.cpp
    kis_crop_processing_visitor_test.cpp
    kis_processing_applicator_test.cpp
    kis_datamanager_test.cpp
    kis_fill_painter_test.cpp
    kis_filter_configuration_test.cpp
    kis_filter_test.cpp
    kis_filter_processing_information_test.cpp
    kis_filter_registry_test.cpp
    kis_filter_strategy_test.cpp
    kis_gradient_painter_test.cpp
    kis_image_commands_test.cpp
    kis_image_test.cpp
    kis_image_signal_router_test.cpp
    kis_iterators_ng_test.cpp
    kis_iterator_benchmark.cpp
    kis_updater_context_test.cpp
    kis_simple_update_queue_test.cpp
    kis_stroke_test.cpp
    kis_simple_stroke_strategy_test.cpp
    kis_stroke_strategy_undo_command_based_test.cpp
    kis_strokes_queue_test.cpp
    kis_mask_test.cpp
    kis_math_toolbox_test.cpp
    kis_name_server_test.cpp
    kis_node_commands_test.cpp
    kis_node_graph_listener_test.cpp
    kis_node_visitor_test.cpp
    kis_paint_information_test.cpp
    kis_distance_information_test.cpp
    kis_paintop_test.cpp
    kis_pattern_test.cpp
    kis_selection_mask_test.cpp
    kis_shared_ptr_test.cpp
    kis_bsplines_test.cpp
    kis_warp_transform_worker_test.cpp
    kis_liquify_transform_worker_test.cpp
    kis_transparency_mask_test.cpp
    kis_types_test.cpp
    kis_vec_test.cpp
    kis_filter_config_widget_test.cpp
    kis_mask_generator_test.cpp
    kis_cubic_curve_test.cpp
    kis_cubic_spline_test.cpp
    kis_fixed_point_maths_test.cpp
    kis_node_query_path_test.cpp
    kis_filter_weights_buffer_test.cpp
    kis_filter_weights_applicator_test.cpp
    kis_fill_interval_test.cpp
    kis_fill_interval_map_test.cpp
    kis_scanline_fill_test.cpp
    kis_psd_layer_style_test.cpp
    kis_layer_style_projection_plane_test.cpp
    kis_lod_capable_layer_offset_test.cpp
    kis_algebra_2d_test.cpp
    kis_marker_painter_test.cpp
    kis_lazy_brush_test.cpp
    kis_mask_similarity_test.cpp
    KisMaskGeneratorTest.cpp
    kis_layer_style_filter_environment_test.cpp
    kis_asl_parser_test.cpp
    KisPerStrokeRandomSourceTest.cpp
    KisWatershedWorkerTest.cpp
    kis_dom_utils_test.cpp
    kis_transform_worker_test.cpp
    kis_cs_conversion_test.cpp
    kis_projection_leaf_test.cpp
    kis_histogram_test.cpp
    kis_onion_skin_compositor_test.cpp
    kis_queues_progress_updater_test.cpp
    kis_image_animation_interface_test.cpp
    kis_walkers_test.cpp
    kis_cage_transform_worker_test.cpp
    KisRandomGenerator2DTest.cpp
    kis_keyframing_test.cpp
    kis_time_span_test.cpp
    kis_filter_mask_test.cpp
    kis_asl_layer_style_serializer_test.cpp
    TestAslStorage.cpp
    kis_async_merger_test.cpp
    kis_selection_test.cpp
    kis_update_scheduler_test.cpp
    kis_colorize_mask_test.cpp
    kis_processings_test.cpp
    kis_paint_device_test.cpp
    kis_layer_styles_test.cpp
    kis_mesh_transform_worker_test.cpp
    KisKeyframeAnimationInterfaceSignalTest.cpp
    KisOverlayPaintDeviceWrapperTest.cpp
    KisPaintOpPresetTest.cpp
    LINK_LIBRARIES kritaimage kritatestsdk
    NAME_PREFIX "libs-image-"
    )

kis_add_test(
    KisMplTest.cpp
    LINK_LIBRARIES kritaimage kritatestsdk kritaglobal
    NAME_PREFIX "libs-image-"
    )

kis_add_test(
    kis_clone_layer_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk kritaglobal kritatransformmaskstubs
    NAME_PREFIX "libs-image-"
    )

kis_add_test(
    kis_transform_mask_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk kritaglobal kritatransformmaskstubs
    NAME_PREFIX "libs-image-"
    )


krita_add_broken_unit_tests(
    kis_perspective_transform_worker_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk
    NAME_PREFIX "libs-image-"
    )

if(APPLE)
    set_property(TARGET kis_image_animation_interface_test PROPERTY MACOSX_BUNDLE ON)
    set_property(TARGET kis_cage_transform_worker_test PROPERTY MACOSX_BUNDLE ON)

    macos_test_fixrpath(KisRandomGenerator2DDemo)
endif()
