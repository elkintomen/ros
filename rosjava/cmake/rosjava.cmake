rosbuild_find_ros_package(genmsg_cpp)
rosbuild_find_ros_package(rosjava)
include(FindJava)

set( _java_classpath "" )
set( _java_runtime_classpath "" )
set( _jniexe_path "" )
set( JAVA_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/bin" )

# Add all the jar files under a given directory to the classpath
macro(add_jar_dir _jardir)
    file(GLOB_RECURSE _jar_files ${_jardir}/*.jar)
    foreach(_jar ${_jar_files})
        add_classpath(${_jar})
        add_runtime_classpath(${_jar})
    endforeach(_jar)
endmacro(add_jar_dir)

# Add all the .jnilib files under a given directory to the classpath
macro(add_jni_path _cp)
	if(EXISTS ${_cp})
		list(APPEND _jniexe_path ${_cp})
	endif(EXISTS ${_cp})
endmacro(add_jni_path _cp) 

# Add a jar or directory to java runtime dependencies. 
macro(add_runtime_classpath _cp)
	if (EXISTS ${_cp})
		list(APPEND _java_runtime_classpath ${_cp})
	endif(EXISTS ${_cp})
endmacro(add_runtime_classpath _cp)

# Add a jar to javac dependencies
macro(add_classpath _cp)
  if(EXISTS ${_cp})
    list(APPEND _java_classpath ${_cp})
  endif(EXISTS ${_cp})
endmacro(add_classpath)

add_classpath(${rosjava_PACKAGE_PATH}/bin)

macro(add_java_source_dir _srcdir)
  add_deps_classpath()
  set(_targetname _java_compile_${JAVA_OUTPUT_DIR})
  string(REPLACE "/" "_" _targetname ${_targetname})  
  add_custom_target(${_targetname} ALL)
  foreach(_cp ${_java_classpath})
    add_java_source_dir_internal(${_targetname} ${_cp})
  endforeach(_cp)
  add_java_source_dir_internal(${_targetname} ${_srcdir})
endmacro(add_java_source_dir)

# Compile java files in _srcdir and put the compiled files in
# _destdir.
macro(add_java_source_dir_internal _targetname _srcdir)
  file(GLOB_RECURSE _java_rel_src_files
    RELATIVE ${_srcdir}
    ${_srcdir}/*.java)
  set(_java_source_files "")
  set(_java_output_files "")
  foreach(_src ${_java_rel_src_files})
    string(REPLACE ".java" ".class" _dest ${_src})
    list(APPEND _java_source_files ${_srcdir}/${_src})
    list(APPEND _java_output_files ${JAVA_OUTPUT_DIR}/${_dest})
  endforeach(_src)
  if(_java_output_files)
    string(REPLACE ";" ":" _javac_classpath_param "${_java_classpath}")
    add_custom_command(
      OUTPUT ${_java_output_files}
      COMMAND ${JAVA_COMPILE} -source 1.5 -classpath ${_javac_classpath_param} -d ${JAVA_OUTPUT_DIR} ${_java_source_files}
      WORKING_DIRECTORY ${_srcdir}
      DEPENDS ${_java_source_files})
  endif(_java_output_files)
  set(_local_targetname ${_targetname}_${_srcdir})
  string(REPLACE "/" "_" _local_targetname ${_local_targetname})
  add_custom_target(${_local_targetname}
    DEPENDS ${_java_output_files})
  add_dependencies(${_local_targetname} rospack_genmsg rospack_gensrv)
  add_dependencies(${_targetname} ${_local_targetname})
endmacro(add_java_source_dir_internal)

macro(add_deps_classpath)
  execute_process(COMMAND rospack depends ${PROJECT_NAME}
                  OUTPUT_VARIABLE _rosjava_deps
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REPLACE "\n" ";" _rosjava_deps ${_rosjava_deps})
  foreach( _dep ${_rosjava_deps} )
    execute_process(COMMAND rospack find ${_dep}
                    OUTPUT_VARIABLE _dep_path
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    add_classpath(${_dep_path}/msg/java)
    add_classpath(${_dep_path}/srv/java)
  endforeach( _dep )
 #  Have to handle msgs/srvs for this package specially since
 #  .java files will not be built until after this script has completed. 
 #  add_classpath( ${PROJECT_SOURCE_DIR}/msg/java )
 #  add_classpath( ${PROJECT_SOURCE_DIR}/srv/java )
endmacro(add_deps_classpath)

macro(rospack_add_java_executable _exe_name _class)
  string(REPLACE ";" ":" _javac_classpath_param "${JAVA_OUTPUT_DIR}:${_java_runtime_classpath}:${rosjava_PACKAGE_PATH}/bin")
  string(REPLACE ";" ":" _jniexe_path "${_jniexe_path}")
  add_custom_command(
    OUTPUT ${EXECUTABLE_OUTPUT_PATH}/${_exe_name}
    COMMAND ${rosjava_PACKAGE_PATH}/scripts/rosjava_gen_exe ${_javac_classpath_param} ${_class} ${EXECUTABLE_OUTPUT_PATH}/${_exe_name} ${_jniexe_path} )
  set(_targetname ${EXECUTABLE_OUTPUT_PATH}/${_exe_name})
  string(REPLACE "/" "_" _targetname ${_targetname})
  add_custom_target(${_targetname} ALL DEPENDS ${EXECUTABLE_OUTPUT_PATH}/${_exe_name})
endmacro(rospack_add_java_executable)

# Message-generation support.
macro(genmsg_java)
  rosbuild_get_msgs(_msglist)
  set(_autogen "")
  foreach(_msg ${_msglist})
    # Construct the path to the .msg file
    set(_input ${PROJECT_SOURCE_DIR}/msg/${_msg})
  
    rosbuild_gendeps(${PROJECT_NAME} ${_msg})
  
    set(genmsg_java_exe ${genmsg_cpp_PACKAGE_PATH}/genmsg_java)
  
    # TODO: Figure a better way to lay out the .java files
    set(_output_java ${PROJECT_SOURCE_DIR}/msg/java/ros/pkg/${PROJECT_NAME}/msg/${_msg})
    string(REPLACE ".msg" ".java" _output_java ${_output_java})
  
    # Add the rule to build the .java from the .msg
    add_custom_command(OUTPUT ${_output_java} 
                       COMMAND ${genmsg_java_exe} ${_input}
                       DEPENDS ${_input} ${genmsg_java_exe} ${gendeps_exe} ${${PROJECT_NAME}_${_msg}_GENDEPS} ${ROS_MANIFEST_LIST})
    list(APPEND _autogen ${_output_java})
  endforeach(_msg)
  # Create a target that depends on the union of all the autogenerated
  # files
  add_custom_target(ROSBUILD_genmsg_java DEPENDS ${_autogen})
  # Add our target to the top-level genmsg target, which will be fired if
  # the user calls genmsg()
  add_dependencies(rospack_genmsg ROSBUILD_genmsg_java)
  list(APPEND _java_classpath ${PROJECT_SOURCE_DIR}/msg/java/)
endmacro(genmsg_java)

# Call the macro we just defined.
genmsg_java()

# Service-generation support.
macro(gensrv_java)
  rosbuild_get_srvs(_srvlist)
  set(_autogen "")
  foreach(_srv ${_srvlist})
    # Construct the path to the .srv file
    set(_input ${PROJECT_SOURCE_DIR}/srv/${_srv})
  
    rosbuild_gendeps(${PROJECT_NAME} ${_srv})
  
    set(gensrv_java_exe ${genmsg_cpp_PACKAGE_PATH}/gensrv_java)

    # TODO: Figure a better way to lay out the .java files
    set(_output_java ${PROJECT_SOURCE_DIR}/srv/java/ros/pkg/${PROJECT_NAME}/srv/${_srv})
  
    string(REPLACE ".srv" ".java" _output_java ${_output_java})
  
    # Add the rule to build the .java from the .srv
    add_custom_command(OUTPUT ${_output_java} 
                       COMMAND ${gensrv_java_exe} ${_input}
                       DEPENDS ${_input} ${gensrv_java_exe} ${gendeps_exe} ${${PROJECT_NAME}_${_srv}_GENDEPS} ${ROS_MANIFEST_LIST})
    list(APPEND _autogen ${_output_java})
  endforeach(_srv)
  # Create a target that depends on the union of all the autogenerated
  # files
  add_custom_target(ROSBUILD_gensrv_java DEPENDS ${_autogen})
  # Add our target to the top-level gensrv target, which will be fired if
  # the user calls gensrv()
  add_dependencies(rospack_gensrv ROSBUILD_gensrv_java)
  list(APPEND _java_classpath ${PROJECT_SOURCE_DIR}/srv/java/)
endmacro(gensrv_java)


# Call the macro we just defined.
gensrv_java()
