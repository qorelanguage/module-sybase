IF(NOT EXISTS "/home/david/src/qore/git/module-sybase/build/install_manifest.txt")
  MESSAGE(FATAL_ERROR "Cannot find install manifest: \"/home/david/src/qore/git/module-sybase/build/install_manifest.txt\"")
ENDIF(NOT EXISTS "/home/david/src/qore/git/module-sybase/build/install_manifest.txt")

EXEC_PROGRAM("xargs rm < /home/david/src/qore/git/module-sybase/build/install_manifest.txt"
            OUTPUT_VARIABLE rm_out
            RETURN_VARIABLE rm_ret)
