;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Software License Agreement (BSD License)
;; 
;; Copyright (c) 2008, Willow Garage, Inc.
;; All rights reserved.
;;
;; Redistribution and use in source and binary forms, with 
;; or without modification, are permitted provided that the 
;; following conditions are met:
;;
;;  * Redistributions of source code must retain the above 
;;    copyright notice, this list of conditions and the 
;;    following disclaimer.
;;  * Redistributions in binary form must reproduce the 
;;    above copyright notice, this list of conditions and 
;;    the following disclaimer in the documentation and/or 
;;    other materials provided with the distribution.
;;  * Neither the name of Willow Garage, Inc. nor the names 
;;    of its contributors may be used to endorse or promote 
;;    products derived from this software without specific 
;;    prior written permission.
;; 
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
;; CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED 
;; WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
;; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
;; PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
;; COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
;; INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
;; CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
;; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
;; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
;; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
;; CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
;; OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
;; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
;; DAMAGE.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


(defpackage :ros-load-manifest
    (:nicknames :ros-load)
  (:export :load-manifest :load-system :asdf-paths-to-add :*current-ros-package* :asdf-ros-search :asdf-ros-pkg-search)
  (:use :cl :s-xml))

(in-package :ros-load-manifest)

(defvar *current-ros-package* nil
  "A string naming the current package.  This is used in the asdf-ros-search search method.")

(defvar *ros-asdf-output-cache* (make-hash-table :test 'eq))

(defvar *ros-asdf-paths-cache* (make-hash-table :test 'equal)
  "Cache of asdf directories returned by ASDF-PATHS-TO-ADD to reduce rospack calls.")

(defclass asdf::roslisp-msg-source-file (asdf:cl-source-file) ())

(defmethod asdf:output-files ((operation asdf:compile-op) (c asdf::roslisp-msg-source-file))
  (labels ((find-system (c)
             "Returns the system of a component."
             (typecase c
               (asdf:system c)
               (asdf:component (find-system (asdf:component-parent c)))))
           (find-pkg-path (path &optional traversed)
             "Traverses the `path' upwards until it finds a manifest.
              Returns two values, the name of the ros package and the
              relative part of path inside the package. Throws an
              error if it cannot find the manifest."
             (let ((manifest (probe-file (merge-pathnames "manifest.xml" path))))
               (if manifest
                   (values (truename path) traversed)
                   (find-pkg-path (make-pathname :directory (butlast (pathname-directory path)))
                                  (cons (car (last (pathname-directory path)))
                                        traversed)))))
           (system-ros-name (system)
             "Returns the ros package name of a system."
             (multiple-value-bind (package-path rel-path)
                 (find-pkg-path (asdf:component-pathname system))
               (assert (eq (car (pathname-directory package-path)) :absolute))
               (values (car (last (pathname-directory package-path))) rel-path)))
           (pathname-rel-subdir (p1 p2)
             "returns the relative path of `p2' in `p1'."
             (loop with result = (pathname-directory  (truename p2))
                   for d1 in (pathname-directory (truename p1))
                   do (setf result (cdr result))
                   finally (return result))))
    (let ((system (find-system c))
          (component-path (asdf:component-pathname c)))
      (destructuring-bind (package-name rel-path)
          (or (gethash system *ros-asdf-output-cache*)
              (setf (gethash system *ros-asdf-output-cache*)
                    (multiple-value-list (system-ros-name system))))
        (list
         (asdf::compile-file-pathname
          (merge-pathnames (make-pathname :name (pathname-name component-path)
                                          :type (pathname-type component-path)
                                          :directory `(:relative ,@(pathname-rel-subdir
                                                                    (asdf:component-pathname system)
                                                                    component-path)))
                           (merge-pathnames
                            (make-pathname :directory `(:relative "roslisp" ,package-name ,@rel-path))
                            (ros-home)))))))))

(defun ros-home ()
  (or (sb-ext:posix-getenv "ROS_HOME")
      (merge-pathnames (make-pathname :directory '(:relative ".ros"))
                       (user-homedir-pathname))))

(defun depended-packages (package-root)
  "Look in this directory for a manifest.xml.  If it doesn't exist, signal an error.  Else, look in the file for tags that look like <depend package=foo> (where foo is a string with double quotes), and return a list of such foo."
  (let ((tree (s-xml:parse-xml-file (merge-pathnames "manifest.xml" package-root))))
    (depended-packages-helper tree)))

(defun depended-packages-helper (tree)
  (when (listp tree)
    (if (eq :|depend| (first tree))
        (list (third tree))
        (mapcan #'depended-packages-helper tree))))


(defun asdf-paths-to-add (package)
  "Given a package name, looks in the manifest and follows dependencies (in a depth-first order).  Stops when it reaches a leaf or a package that contains no asdf/ directory.  Adds all the /asdf directories that it finds to a list and return it."
  (let ((already-seen-packages nil))
    (labels ((helper (package)
               (unless (member package already-seen-packages :test #'equal)
                 (let* ((path (ros-package-path package))
                        (asdf-dir (get-asdf-directory path)))
                   (push package already-seen-packages)
                   (append
                    (when asdf-dir (list asdf-dir))
                    (mapcan #'helper (depended-packages path)))))))
      (or (gethash package *ros-asdf-paths-cache*)
          (setf (gethash package *ros-asdf-paths-cache*)
                (helper package))))))


(defun normalize (str)
  (let* ((pos (position #\Newline str))
         (stripped (if pos
                       (subseq str 0 pos)
                       str)))
    (if (eq #\/ (char stripped (1- (length stripped))))
        stripped
        (concatenate 'string stripped "/"))))

(defun ros-package-path (p)
  (let* ((str (make-string-output-stream))
         (error-str (make-string-output-stream))
         (proc (sb-ext:run-program "rospack" (list "find" p) :search t :output str :error error-str))
         (exit-code (sb-ext:process-exit-code proc)))
    (if (zerop exit-code)
        (pathname (normalize (get-output-stream-string str)))
        (error "rospack find ~a returned ~a with stderr '~a'" 
               p exit-code (get-output-stream-string error-str)))))


(defun get-asdf-directory (path)
  (let ((asdf-path (merge-pathnames "asdf/" path)))
    (when (probe-file asdf-path) asdf-path)))

(defun asdf-ros-msg-srv-search (definition)
  "An ASDF search method for the systems containing autogenerated definitions of ros message and service types"
  (setq definition (asdf::coerce-name definition))
  (when (> (length definition) 4)
    (let ((package-name (subseq definition 0 (- (length definition) 4)))
          (package-suffix (subseq definition (- (length definition) 4))))
      (when (member package-suffix '("-msg" "-srv") :test #'equal)
        (let ((filename (merge-pathnames 
                         (make-pathname
                          :directory `(:relative ,(subseq package-suffix 1)
                                                 "lisp" ,package-name)
                          :name definition
                          :type "asd")
                         (parse-namestring (ros-package-path package-name)))))
          (when (probe-file filename)
            filename))))))


(defun asdf-ros-search (def &aux (debug-print (sb-ext:posix-getenv "ROSLISP_LOAD_DEBUG")))
  "An ASDF search method for ros packages.  When *current-ros-package* is a nonempty string, it uses rospack to generate the list of depended-upon packages, with the current one at the front.  It then searches the asdf/ subdirectory of each package root in turn for the package."
  (if (and (stringp *current-ros-package*) (> (length *current-ros-package*) 0))
      (let ((paths (asdf-paths-to-add *current-ros-package*)))
        (when debug-print (format t "~&Current ros package is ~a.  Searching for asdf system ~a in directories:~&    ~a" *current-ros-package* def paths))
        (dolist (p paths)
          (let ((filename (merge-pathnames (make-pathname :name def :type "asd") p)))
            (when (probe-file filename)
              (when debug-print (format t "~&  Found ~a" filename))
              (return-from asdf-ros-search filename))))
        (when debug-print (format t "~&  Not found")))
      (when debug-print (format t "~&asdf-ros-search not invoked since *current-ros-package* is ~a" *current-ros-package*))))

(defun asdf-ros-pkg-search (definition &aux (debug-print (sb-ext:posix-getenv "ROSLISP_LOAD_DEBUG")))
  "An ASDF search method that searches for something like foo/bar in subdirectory asdf/bar.asd of ros package foo.  The system name must also be foo/bar in this case"
  (when debug-print (format t "~&Looking for system ~a" definition))
  (setq definition (asdf::coerce-name definition))
  (let ((pos (position #\/ definition :from-end t)))
    (when pos
      (let* ((pkg (subseq definition 0 pos))
             (suffix (subseq definition (1+ pos)))
             (pkg-path (parse-namestring (ros-package-path pkg)))
             (filename
              (merge-pathnames
               (make-pathname :directory '(:relative "asdf") :name suffix :type "asd")
               pkg-path)))
        (when debug-print (format t "~&  Checking if ~a exists" filename))
        (when (probe-file filename)
          (when debug-print (format t "~&  ...found!"))
          filename)))))


(setq asdf:*system-definition-search-functions* 
      (append asdf:*system-definition-search-functions*
              '(asdf-ros-msg-srv-search asdf-ros-pkg-search asdf-ros-search)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; top level
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun load-manifest (package)
  "Walks down the tree of dependencies of this ros package.  Backtracks when it reaches a leaf or a package with no asdf/ subdirectory.  Adds all the asdf directories it finds to the asdf:*central-registry*."
  (cerror "continue" "Load manifest deprecated!")
  (dolist (p (asdf-paths-to-add package))
    (pushnew p asdf:*central-registry* :test #'equal)))

(defun load-system (package &optional (asdf-name package))
  "Sets *CURRENT-ROS-PACKAGE* and performs an asdf load operation on `package'"
  (let ((*current-ros-package* package))
    (asdf:operate 'asdf:load-op asdf-name)))