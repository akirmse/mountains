#
# Makefile for peak database manipulation programs
#

AR     = ar -r
CC     = gcc
MAKE   = make
LINK   = g++ 

CP = cp
RM = rm -f
RMDIR = rmdir
MV = mv
MKDIR = mkdir -p

SOURCEDIR = .

ifdef RELEASE
DEBUG=
OUTDIR=release
else
DEBUG = 1
OUTDIR=debug
endif

LIBS =

# Disable easylogging++ logging to a file by default
CCOMMONFLAGS = -DPLATFORM_LINUX -Wall -Werror -Wno-unused-variable -std=c++14 \
	-DELPP_NO_DEFAULT_LOG_FILE -DELPP_THREAD_SAFE -pthread
CNORMALFLAGS = $(CCOMMONFLAGS) -O3
CDEBUGFLAGS = $(CCOMMONFLAGS) -g
LINKCOMMONFLAGS = -pthread
LINKNORMALFLAGS = $(LINKCOMMONFLAGS)
LINKDEBUGFLAGS = $(LINKCOMMONFLAGS)

ifdef DEBUG

CFLAGS = $(CDEBUGFLAGS)
LINKFLAGS = $(LINKDEBUGFLAGS)

else
CFLAGS = $(CNORMALFLAGS)
LINKFLAGS = $(LINKNORMALFLAGS)
endif

POINTLIB = $(OUTDIR)/pointlib.a
LOADERLIB = $(OUTDIR)/loaderlib.a

POINTLIB_OBJS = \
	$(OUTDIR)/point.o \
	$(OUTDIR)/quadtree.o \
	$(OUTDIR)/util.o \

LOADERLIB_OBJS = \
	$(OUTDIR)/flt_loader.o \
	$(OUTDIR)/glo_loader.o \
	$(OUTDIR)/hgt_loader.o \

ISOLATION_OBJS = \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/file_format.o \
	$(OUTDIR)/isolation.o \
	$(OUTDIR)/isolation_finder.o \
	$(OUTDIR)/isolation_results.o \
	$(OUTDIR)/isolation_task.o \
	$(OUTDIR)/latlng.o \
	$(OUTDIR)/peak_finder.o \
	$(OUTDIR)/peakbagger_collection.o \
	$(OUTDIR)/peakbagger_point.o \
	$(OUTDIR)/point_map.o \
	$(OUTDIR)/tile.o \
	$(OUTDIR)/tile_cache.o \
	$(OUTDIR)/tile_loading_policy.o \
	$(LOADERLIB) \
	$(POINTLIB) \

PROMINENCE_OBJS = \
	$(OUTDIR)/coordinate_system.o \
	$(OUTDIR)/divide_tree.o \
	$(OUTDIR)/domain_map.o \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/filter.o \
	$(OUTDIR)/file_format.o \
	$(OUTDIR)/island_tree.o \
	$(OUTDIR)/kml_writer.o \
	$(OUTDIR)/latlng.o \
	$(OUTDIR)/line_tree.o \
	$(OUTDIR)/peakbagger_collection.o \
	$(OUTDIR)/peakbagger_point.o \
	$(OUTDIR)/point_map.o \
	$(OUTDIR)/prominence.o \
	$(OUTDIR)/prominence_task.o \
	$(OUTDIR)/tile.o \
	$(OUTDIR)/tile_cache.o \
	$(OUTDIR)/tile_loading_policy.o \
	$(OUTDIR)/tree_builder.o \
	$(LOADERLIB) \
	$(POINTLIB) \

MERGE_DIVIDE_TREES_OBJS = \
	$(OUTDIR)/coordinate_system.o \
	$(OUTDIR)/divide_tree.o \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/island_tree.o \
	$(OUTDIR)/kml_writer.o \
	$(OUTDIR)/latlng.o \
	$(OUTDIR)/line_tree.o \
	$(OUTDIR)/merge_divide_trees.o \
	$(OUTDIR)/tile.o \
	$(POINTLIB) \

FILTER_POINTS_OBJS = \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/filter.o \
	$(OUTDIR)/filter_points.o \
	$(POINTLIB) \

COMPUTE_PARENTS_OBJS = \
	$(OUTDIR)/coordinate_system.o \
	$(OUTDIR)/divide_tree.o \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/island_tree.o \
	$(OUTDIR)/kml_writer.o \
	$(OUTDIR)/line_tree.o \
	$(OUTDIR)/compute_parents.o \
	$(POINTLIB) \


all : makedirs $(OUTDIR)/isolation $(OUTDIR)/prominence $(OUTDIR)/merge_divide_trees \
	 $(OUTDIR)/filter_points $(OUTDIR)/compute_parents

$(POINTLIB) : $(POINTLIB_OBJS)
	$(AR) $@ $^ 

$(LOADERLIB): $(LOADERLIB_OBJS)
	$(AR) $@ $^ 

$(OUTDIR)/isolation: $(ISOLATION_OBJS)
	$(LINK) $^ $(LIBS) -o $@ $(LINKFLAGS)

$(OUTDIR)/prominence: $(PROMINENCE_OBJS)
	$(LINK) $^ -o $@ $(LIBS) $(LINKFLAGS)

$(OUTDIR)/merge_divide_trees: $(MERGE_DIVIDE_TREES_OBJS)
	$(LINK) $^ -o $@ $(LIBS) $(LINKFLAGS)

$(OUTDIR)/filter_points: $(FILTER_POINTS_OBJS)
	$(LINK) $^ $(LIBS) -o $@ $(LINKFLAGS)

$(OUTDIR)/compute_parents: $(COMPUTE_PARENTS_OBJS)
	$(LINK) $^ $(LIBS) -o $@ $(LINKFLAGS)

$(OUTDIR)/%.o : $(SOURCEDIR)/%.cpp
	$(CC) $(CFLAGS) -I $(SOURCEDIR) -o $@ -c $< 

$(OUTDIR)/%.o : $(SOURCEDIR)/%.cc
	$(CC) $(CFLAGS) -I $(SOURCEDIR) -o $@ -c $< 

$(OUTDIR)/%.o : $(OUTDIR)/%.cpp
	$(CC) $(CFLAGS) -I $(SOURCEDIR) -o $@ -c $< 

makedirs:
	-@$(MKDIR) $(OUTDIR)

clean:
	@$(RM) $(OUTDIR)/*
	@$(RMDIR) $(OUTDIR)

depend:
	makedepend -Y -p'$(OUTDIR)/' *.cpp
	@$(RM) Makefile.bak

# DO NOT DELETE

debug/compute_parents.o: divide_tree.h coordinate_system.h primitives.h
debug/compute_parents.o: latlng.h island_tree.h line_tree.h util.h
debug/compute_parents.o: easylogging++.h
debug/coordinate_system.o: coordinate_system.h primitives.h latlng.h
debug/divide_tree.o: divide_tree.h coordinate_system.h primitives.h latlng.h
debug/divide_tree.o: easylogging++.h island_tree.h kml_writer.h line_tree.h
debug/divide_tree.o: util.h
debug/domain_map.o: domain_map.h tile.h primitives.h latlng.h pixel_array.h
debug/domain_map.o: easylogging++.h
debug/filter.o: easylogging++.h filter.h latlng.h util.h
debug/filter_points.o: easylogging++.h filter.h latlng.h util.h
debug/flt_loader.o: flt_loader.h tile_loader.h tile.h primitives.h latlng.h
debug/flt_loader.o: util.h easylogging++.h
debug/glo_loader.o: glo_loader.h tile_loader.h tile.h primitives.h latlng.h
debug/glo_loader.o: util.h easylogging++.h
debug/hgt_loader.o: hgt_loader.h tile_loader.h tile.h primitives.h latlng.h
debug/hgt_loader.o: util.h easylogging++.h
debug/island_tree.o: island_tree.h primitives.h divide_tree.h
debug/island_tree.o: coordinate_system.h latlng.h easylogging++.h
debug/island_tree.o: kml_writer.h
debug/isolation.o: isolation_task.h tile_cache.h lock.h lrucache.h
debug/isolation.o: point_map.h point.h tile.h primitives.h latlng.h
debug/isolation.o: tile_loading_policy.h peakbagger_collection.h
debug/isolation.o: peakbagger_point.h quadtree.h ThreadPool.h easylogging++.h
debug/isolation_finder.o: isolation_finder.h tile_cache.h lock.h lrucache.h
debug/isolation_finder.o: point_map.h point.h tile.h primitives.h latlng.h
debug/isolation_finder.o: tile_loading_policy.h easylogging++.h math_util.h
debug/isolation_point.o: isolation_point.h point.h latlng.h
debug/isolation_results.o: isolation_results.h latlng.h
debug/isolation_task.o: isolation_finder.h tile_cache.h lock.h lrucache.h
debug/isolation_task.o: point_map.h point.h tile.h primitives.h latlng.h
debug/isolation_task.o: tile_loading_policy.h isolation_task.h
debug/isolation_task.o: isolation_results.h peak_finder.h easylogging++.h
debug/kml_writer.o: kml_writer.h primitives.h coordinate_system.h latlng.h
debug/latlng.o: latlng.h math_util.h
debug/line_tree.o: line_tree.h primitives.h divide_tree.h coordinate_system.h
debug/line_tree.o: latlng.h easylogging++.h
debug/merge_divide_trees.o: divide_tree.h coordinate_system.h primitives.h
debug/merge_divide_trees.o: latlng.h island_tree.h easylogging++.h
debug/multi_source_loading_policy.o: multi_source_loading_policy.h
debug/multi_source_loading_policy.o: tile_loading_policy.h tile.h
debug/multi_source_loading_policy.o: primitives.h latlng.h
debug/peak_finder.o: peak_finder.h tile.h primitives.h latlng.h
debug/peakbagger_collection.o: peakbagger_collection.h peakbagger_point.h
debug/peakbagger_collection.o: point.h quadtree.h
debug/peakbagger_point.o: peakbagger_point.h point.h
debug/point.o: point.h
debug/point_map.o: point_map.h point.h easylogging++.h
debug/prominence.o: filter.h latlng.h peakbagger_collection.h
debug/prominence.o: peakbagger_point.h point.h quadtree.h point_map.h
debug/prominence.o: prominence_task.h tile_cache.h lock.h lrucache.h tile.h
debug/prominence.o: primitives.h tile_loading_policy.h ThreadPool.h
debug/prominence.o: easylogging++.h
debug/prominence_collection.o: prominence_collection.h prominence_point.h
debug/prominence_collection.o: point.h latlng.h quadtree.h
debug/prominence_point.o: prominence_point.h point.h latlng.h
debug/prominence_task.o: prominence_task.h tile_cache.h lock.h lrucache.h
debug/prominence_task.o: point_map.h point.h tile.h primitives.h latlng.h
debug/prominence_task.o: tile_loading_policy.h divide_tree.h
debug/prominence_task.o: coordinate_system.h island_tree.h tree_builder.h
debug/prominence_task.o: domain_map.h pixel_array.h easylogging++.h
debug/quadtree.o: quadtree.h point.h
debug/tile.o: tile.h primitives.h latlng.h math_util.h util.h easylogging++.h
debug/tile_cache.o: tile_cache.h lock.h lrucache.h point_map.h point.h tile.h
debug/tile_cache.o: primitives.h latlng.h tile_loading_policy.h
debug/tile_cache.o: peakbagger_point.h easylogging++.h
debug/tile_loading_policy.o: tile_loading_policy.h tile.h primitives.h
debug/tile_loading_policy.o: latlng.h flt_loader.h tile_loader.h util.h
debug/tile_loading_policy.o: easylogging++.h glo_loader.h hgt_loader.h
debug/tree_builder.o: tree_builder.h primitives.h domain_map.h tile.h
debug/tree_builder.o: latlng.h pixel_array.h divide_tree.h
debug/tree_builder.o: coordinate_system.h easylogging++.h
debug/util.o: util.h
