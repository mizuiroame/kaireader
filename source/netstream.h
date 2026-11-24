// Books of the network folder ("dav:/..." paths) opened by MuPDF over the
// network, reading only the parts it asks for (netstream.c).
#pragma once

// Makes doc.c open "dav:/..." paths this way.
void netStreamInstall(void);
