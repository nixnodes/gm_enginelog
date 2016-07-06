# gm_enginelog

##### Asynchronous logging module based on enginespew 

-

By default, logging is disabled, use:

  `ELog.EnableLogging(true)`
  
Enginespew must be loaded for this to work.

-

Captured spew is written to <srcds_linux base>/logs/enginelog.<timestamp>.log (5MB max file size)
