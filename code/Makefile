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

ifdef DEBUG
DEBUG = 1
OUTDIR=debug
else
DEBUG=
OUTDIR=release
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

COORDLIB = $(OUTDIR)/coordlib.a
LOADERLIB = $(OUTDIR)/loaderlib.a
POINTLIB = $(OUTDIR)/pointlib.a

POINTLIB_OBJS = \
	$(OUTDIR)/latlng.o \
	$(OUTDIR)/util.o \

LOADERLIB_OBJS = \
	$(OUTDIR)/fabdem_loader.o \
	$(OUTDIR)/flt_loader.o \
	$(OUTDIR)/glo_loader.o \
	$(OUTDIR)/hgt_loader.o \

COORDLIB_OBJS = \
	$(OUTDIR)/coordinate_system.o \
	$(OUTDIR)/degree_coordinate_system.o \
	$(OUTDIR)/utm_coordinate_system.o \

ISOLATION_OBJS = \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/file_format.o \
	$(OUTDIR)/isolation.o \
	$(OUTDIR)/isolation_finder.o \
	$(OUTDIR)/isolation_results.o \
	$(OUTDIR)/isolation_task.o \
	$(OUTDIR)/peak_finder.o \
	$(OUTDIR)/tile.o \
	$(OUTDIR)/tile_cache.o \
	$(OUTDIR)/tile_loading_policy.o \
	$(LOADERLIB) \
	$(POINTLIB) \
	$(COORDLIB) \

PROMINENCE_OBJS = \
	$(OUTDIR)/divide_tree.o \
	$(OUTDIR)/domain_map.o \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/filter.o \
	$(OUTDIR)/file_format.o \
	$(OUTDIR)/island_tree.o \
	$(OUTDIR)/kml_writer.o \
	$(OUTDIR)/line_tree.o \
	$(OUTDIR)/prominence.o \
	$(OUTDIR)/prominence_task.o \
	$(OUTDIR)/tile.o \
	$(OUTDIR)/tile_cache.o \
	$(OUTDIR)/tile_loading_policy.o \
	$(OUTDIR)/tree_builder.o \
	$(LOADERLIB) \
	$(POINTLIB) \
	$(COORDLIB) \

MERGE_DIVIDE_TREES_OBJS = \
	$(OUTDIR)/divide_tree.o \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/island_tree.o \
	$(OUTDIR)/kml_writer.o \
	$(OUTDIR)/line_tree.o \
	$(OUTDIR)/merge_divide_trees.o \
	$(OUTDIR)/tile.o \
	$(POINTLIB) \
	$(COORDLIB) \

FILTER_POINTS_OBJS = \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/filter.o \
	$(OUTDIR)/filter_points.o \
	$(POINTLIB) \

COMPUTE_PARENTS_OBJS = \
	$(OUTDIR)/divide_tree.o \
	$(OUTDIR)/easylogging++.o \
	$(OUTDIR)/island_tree.o \
	$(OUTDIR)/kml_writer.o \
	$(OUTDIR)/line_tree.o \
	$(OUTDIR)/compute_parents.o \
	$(POINTLIB) \
	$(COORDLIB) \


all : makedirs $(OUTDIR)/isolation $(OUTDIR)/prominence $(OUTDIR)/merge_divide_trees \
	 $(OUTDIR)/filter_points $(OUTDIR)/compute_parents

$(POINTLIB) : $(POINTLIB_OBJS)
	$(AR) $@ $^ 

$(LOADERLIB): $(LOADERLIB_OBJS)
	$(AR) $@ $^ 

$(COORDLIB): $(COORDLIB_OBJS)
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

makedirs:
	-@$(MKDIR) $(OUTDIR)

clean:
	@$(RM) $(OUTDIR)/*
	@$(RMDIR) $(OUTDIR)

depend:
	makedepend -Y -p'$(OUTDIR)/' *.cpp
	@$(RM) Makefile.bak
