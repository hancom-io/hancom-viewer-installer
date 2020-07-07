typedef enum
{
  STATUS_NORMAL,
  STATUS_DOWNLOADING,
  STATUS_DOWNLOADED,
  STATUS_INSTALLING,
  STATUS_INSTALLED,
  STATUS_CANCEL,
  STATUS_PROGRESS,
  STATUS_ERROR,
  N_STATUS
} InstallStatus;

#define VIEWER_NAME "hoffice-hwpviewer"
#define VIEWER_CHECK "hancom-viewer-check"
#define VIEWER_SCRIPT "hancom-viewer-install"
#define VIEWER_SCHEMA "kr.hancom.viewer-installer"
#define VIEWER_SCHEMA_KEY "never-see-again"
#define VIEWER_REFERER  "https://www.hancom.com/cs_center"
#define VIEWER_INSTALL_URL "https://cdn.hancom.com/pds/hnc/VIE"
