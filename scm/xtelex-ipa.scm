;;
;; This file is a generated file. Don't edit this file.
;;
;; For more details please visit the project home page
;;   https://github.com/icy/myquartz-scim2uim
;;   https://github.com/TheSLinux-forks/myquartz-scim2uim
;;
;; This work is distributed under the license GPL (v2).
;; You must read and accept the license if you want to
;; use, distribute, modify this work, and/or to create
;; any derivative work. The license can be found at
;;   http://www.gnu.org/licenses/gpl-2.0.html
;;
(require "generic.scm")
(define xtelex-ipa-rule
  '(
((("A" ))("A"))
  ))
(define xtelex-ipa-init-handler
  (lambda (id im arg)
    (generic-context-new id im xtelex-ipa-rule #f)))

(generic-register-im
  'xtelex-ipa
  "vi"
  "UTF-8"
  (N_ "XTelex-IPA")
  (N_ "Big table of predefined words for Vietnamese Telex-IPA users")
  xtelex-ipa-init-handler)
