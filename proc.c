#include "memstream.h"

#include <stdint.h>
#include <Windows.h>
#include <leechcore.h>
#include <vmmdll.h>


// MSS_GetProcess returns the first matching process
HRESULT MSS_GetProcess(PMSSContext ctx, const char* name, PMSSProcess* pProcess) {
    if(!ctx || !ctx->hVMM) return E_UNEXPECTED;
    if(!name) return E_INVALIDARG;
    if(!pProcess) return E_INVALIDARG;

   DWORD pid = 0;

    if(!VMMDLL_PidGetFromName(ctx->hVMM, name, &pid))
        return E_FAIL;

    // construct process & return OK
    *pProcess = malloc(sizeof(MSSProcess));
    (*pProcess)->ctx = ctx;
    (*pProcess)->pid = pid;

    return S_OK;
}

// MSS_GetAllProcesses returns a linked list of all processes with the following name
HRESULT MSS_GetAllProcesses(PMSSContext ctx, const char* name, PProcess* pProcessList) {
    if(!ctx || !ctx->hVMM) return E_UNEXPECTED;
    if(!name) return E_INVALIDARG;
    if(!pProcessList) return E_INVALIDARG;

    PDWORD pdwPIDs = 0;
    ULONG64 cPIDs = 0;

    if(!VMMDLL_PidList(ctx->hVMM, NULL, &cPIDs)) return E_FAIL;

    pdwPIDs = (PDWORD)LocalAlloc(LMEM_ZEROINIT, cPIDs * sizeof(DWORD));
    if (!VMMDLL_PidList(ctx->hVMM, pdwPIDs, &cPIDs))
    {
        LocalFree((HLOCAL)pdwPIDs);
        return E_FAIL;
    }

    Process* current = NULL;

    for (DWORD i = 0; i < cPIDs; i++)
    {
        DWORD dwPid = pdwPIDs[i];

        VMMDLL_PROCESS_INFORMATION info = {};
        SIZE_T cbInfo = sizeof(VMMDLL_PROCESS_INFORMATION);
        info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
        info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;
        if (!VMMDLL_ProcessGetInformation(ctx->hVMM, dwPid, &info, &cbInfo))
        {
            continue;
        }
        if(!strcmp(info.szNameLong, name))
        {
            // push entry to end of linked list (or create head if first hit)
            Process* new = (Process*)malloc(sizeof(Process));
            new->next = NULL;
            new->value = (PMSSProcess)(sizeof(MSSProcess));
            new->value->pid = dwPid;
            new->value->ctx = ctx;
            if(current) {
                current->next = new;
            } else {
                *pProcessList = new;
            }
            current = new;
        }
    }

    LocalFree((HLOCAL)pdwPIDs);

    return S_OK;
}

// MSS_GetModuleBase returns the base address of a module in the process
HRESULT MSS_GetModuleBase(PMSSProcess process, const char* name, uint64_t* pBase) {
    if(!process || !process->ctx || !process->ctx->hVMM) return E_UNEXPECTED;
    if(!name) return E_INVALIDARG;
    if(!pBase) return E_INVALIDARG;

    *pBase = VMMDLL_ProcessGetModuleBaseU(process->ctx->hVMM, process->pid, name);
    if(!*pBase) return E_FAIL;

    return S_OK;
}

// MSS_GetModuleExports returns a linked list of module exports from the export address table.
HRESULT MSS_GetModuleExports(PMSSProcess process, const char* name, PExport* pExportList) {
    if(!process || !process->ctx || !process->ctx->hVMM) return E_UNEXPECTED;
    if(!name) return E_INVALIDARG;
    if(!pExportList) return E_INVALIDARG;

    PVMMDLL_MAP_EAT pEatMap = NULL;
    PVMMDLL_MAP_EATENTRY pEatMapEntry;

    Export* current = NULL;

    if(!VMMDLL_Map_GetEATU(process->ctx->hVMM, process->pid, name, &pEatMap))
        return E_FAIL;

    if(pEatMap->dwVersion != VMMDLL_MAP_EAT_VERSION) {
        VMMDLL_MemFree(pEatMap);
        return E_FAIL;
    }
    for(int i = 0; i < pEatMap->cMap; i++) {
        pEatMapEntry = pEatMap->pMap + i;

        Export* new = new = (Export*)malloc(sizeof(Export));
        new->next = NULL;
        new->name = malloc(strlen(pEatMapEntry->uszFunction)+1);
        strcpy_s(new->name, strlen(pEatMapEntry->uszFunction)+1, pEatMapEntry->uszFunction);
        new->address = pEatMapEntry->vaFunction;
        if(current) {
            current->next = new;
        } else {
            *pExportList = new;
        }

        current = new;
    }

    VMMDLL_MemFree(pEatMap);
    return S_OK;
}

// MSS_GetModuleImports returns a linked list of module imports from the import address table.
HRESULT MSS_GetModuleImports(PMSSProcess process, const char* name, PImport* pImportList) {
    if(!process || !process->ctx || !process->ctx->hVMM) return E_UNEXPECTED;
    if(!name) return E_INVALIDARG;
    if(!pImportList) return E_INVALIDARG;

    PVMMDLL_MAP_IAT pIatMap = NULL;
    PVMMDLL_MAP_IATENTRY pIatMapEntry;

    if(!VMMDLL_Map_GetIATU(process->ctx->hVMM, process->pid, name, &pIatMap))
        return E_FAIL;

    if(pIatMap->dwVersion != VMMDLL_MAP_IAT_VERSION) {
        VMMDLL_MemFree(pIatMap);
        return E_FAIL;
    }

    Import* current = NULL;

    for(int i = 0; i < pIatMap->cMap; i++) {
        pIatMapEntry = pIatMap->pMap + i;

        Import* new = (Import*)malloc(sizeof(Import));
        new->next = NULL;
        new->name = malloc(strlen(pIatMapEntry->uszFunction)+1);
        strcpy_s(new->name, strlen(pIatMapEntry->uszFunction)+1, pIatMapEntry->uszFunction);
        new->address = pIatMapEntry->vaFunction;
        if(current) {
            current->next = new;
        } else {
            *pImportList = new;
        }

        current = new;
    }

    VMMDLL_MemFree(pIatMap);
    return S_OK;
}

// TODO:
HRESULT MSS_GetProcessModules(PMSSProcess process, PModule* pModuleList) {
    return E_NOTIMPL;
}