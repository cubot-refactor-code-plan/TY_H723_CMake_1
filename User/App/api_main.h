#ifndef __API_MAIN_H__
#define __API_MAIN_H__

#ifdef __cplusplus
extern "C"
{
#endif

  void app_init(void);

  void _defaultTask(void *argument);
  void _transmitTask(void *argument);
  void _receiveTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif // __API_MAIN_H__
