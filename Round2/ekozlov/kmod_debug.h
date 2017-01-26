#ifndef KMOD_DEBUG_H_
#define KMOD_DEBUG_H_


#define EK_DRV_TAG	"<ek>: "

#define FNIN	pr_info(EK_DRV_TAG "--> %s <%d>\n", __func__, current->pid);
#define FNTRACE pr_info(EK_DRV_TAG "--- %s: %d\n", __func__, __LINE__);
#define FNOUT	pr_info(EK_DRV_TAG "<-- %s <%d>\n", __func__, current->pid);
#define FNOUTRC	pr_info(EK_DRV_TAG "<-- %s: rc = %d <%d>\n", __func__, rc, current->pid);


#endif /* KMOD_DEBUG_H_ */
