;;
;;  Copyright (c) 2005, Konosuke Watanabe <nosuke@csc.ne.jp>
;;  All rights reserved.
;;
;;  Redistribution and use in source and binary forms, with or
;;  without modification, are permitted provided that the
;;  following conditions are met:
;;
;;  1. Redistributions of source code must retain the above
;;     copyright notice, this list of conditions and the
;;     following disclaimer.
;;  2. Redistributions in binary form must reproduce the above
;;     copyright notice, this list of conditions and the
;;     following disclaimer in the documentation and/or other
;;     materials provided with the distribution.
;;  3. Neither the name of authors nor the names of its
;;     contributors may be used to endorse or promote products
;;     derived from this software without specific prior written
;;     permission.
;;
;;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
;;  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
;;  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
;;  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;;  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
;;  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;;  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
;;  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
;;  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
;;  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
;;  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
;;  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
;;  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;

(require 'uim)

;; alist of LEIM style IM names and its Uim style IM names
;;  ( japanese-anthy-uim . anthy )
(defvar uim-leim-inputmethod-alist '())

;; LEIM status
(uim-deflocalvar uim-leim-active nil)


;; Inactivate function
(defun uim-leim-inactivate ()
  (setq uim-leim-active nil)
  (uim-mode-off))

(defadvice toggle-input-method (around uim-toggle-input-method-around activate)
  (if buffer-read-only
      (not (message "uim.el: This buffer is read-only."))
    ad-do-it))

;; Activate function (callback?)
;;  all Uim related LEIM input methods call this function at activation time
(defun uim-leim-activate (&optional name)

  ;; copy toggle-input-method bindings to uim-mode-map
  (uim-update-keybind)

  (let (im)
    ;; register inactivation function
    (setq inactivate-current-input-method-function 'uim-leim-inactivate)

    ;; get plain IM engine name from LEIM style name
    ;;  ex. "Japanese-anthy-uim" => "anthy"
    (setq im (cdr (assoc name uim-leim-inputmethod-alist)))

    (when (uim-mode-on)
      ;; switch IM after Uim activation
      (if (not (equal im uim-current-im-engine))
	  (uim-change-im im))

      (setq uim-leim-active t))))


(defun uim-leim-reset ()
  (when uim-leim-active
    (message "uim.el: uim-leim-reset")
    (inactivate-input-method)))


(defun uim-leim-make-im-name (im)
  (let (lang)
    (setq lang (downcase (uim-get-emacs-lang im)))
    (concat lang "-" im "-uim")))


(defun uim-leim-init ()
  ;; register IM to input-method-alist
  ;;   to display the alist, call list-input-methods
  (let ((im-list (cdr uim-im-list)) lang name im)
    (while im-list
      (setq name (caar im-list))
      (setq lang (cdr (assoc 'emacs-lang (cdar im-list))))

      (when (and name lang)
	(setq im (uim-leim-make-im-name name))
	(register-input-method im lang 'uim-leim-activate "[Uim]"
			       (concat "Uim " name))

	;; ( japanese-anthy-uim . anthy )
	(setq uim-leim-inputmethod-alist 
	      (cons (cons im name) uim-leim-inputmethod-alist)))
      (setq im-list (cdr im-list))))

  (add-hook 'uim-update-default-engine-hook 
	    (lambda ()
	      (if (boundp 'default-input-method)
		  (setq default-input-method
			(uim-leim-make-im-name
			 uim-context-default-im-engine)))))

  (add-hook 'uim-update-current-engine-hook 
	    (lambda ()
	      (if (and (boundp 'current-input-method)
		       current-input-method)
		  (setq current-input-method
			(uim-leim-make-im-name uim-current-im-engine)))))

  (add-hook 'uim-force-inactivate-hook
	    (lambda ()
	      (when uim-leim-active
		(message "uim.el: LEIM inactivated"
			 (inactivate-input-method)))))

  (add-hook 'uim-buffer-init-hook 
	    (lambda ()
	      (add-hook 'change-major-mode-hook
			'inactivate-input-method nil t)))
	       
  )


;; Overwrite
(defun uim-im-switch (&optional im)
  (interactive)
  (message "uim.el: use \"M-x set-input-method\" when using LEIM"))

(defun uim-update-keybind ()
  (uim-copy-toggle-key 'toggle-input-method))

(uim-leim-init)
  
(provide 'uim-leim)
