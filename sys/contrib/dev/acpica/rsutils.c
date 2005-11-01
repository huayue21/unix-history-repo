/*******************************************************************************
 *
 * Module Name: rsutils - Utilities for the resource manager
 *              $Revision: 1.50 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/


#define __RSUTILS_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acresrc.h>


#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsutils")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDecodeBitmask
 *
 * PARAMETERS:  Mask            - Bitmask to decode
 *              List            - Where the converted list is returned
 *
 * RETURN:      Count of bits set (length of list)
 *
 * DESCRIPTION: Convert a bit mask into a list of values
 *
 ******************************************************************************/

UINT8
AcpiRsDecodeBitmask (
    UINT16                  Mask,
    UINT8                   *List)
{
    ACPI_NATIVE_UINT        i;
    UINT8                   BitCount;


    /* Decode the mask bits */

    for (i = 0, BitCount = 0; Mask; i++)
    {
        if (Mask & 0x0001)
        {
            List[BitCount] = (UINT8) i;
            BitCount++;
        }

        Mask >>= 1;
    }

    return (BitCount);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsEncodeBitmask
 *
 * PARAMETERS:  List            - List of values to encode
 *              Count           - Length of list
 *
 * RETURN:      Encoded bitmask
 *
 * DESCRIPTION: Convert a list of values to an encoded bitmask
 *
 ******************************************************************************/

UINT16
AcpiRsEncodeBitmask (
    UINT8                   *List,
    UINT8                   Count)
{
    ACPI_NATIVE_UINT        i;
    UINT16                  Mask;


    /* Encode the list into a single bitmask */

    for (i = 0, Mask = 0; i < Count; i++)
    {
        Mask |= (0x0001 << List[i]);
    }

    return (Mask);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsMoveData
 *
 * PARAMETERS:  Destination         - Pointer to the destination descriptor
 *              Source              - Pointer to the source descriptor
 *              ItemCount           - How many items to move
 *              MoveType            - Byte width
 *
 * RETURN:      None
 *
 * DESCRIPTION: Move multiple data items from one descriptor to another. Handles
 *              alignment issues and endian issues if necessary, as configured
 *              via the ACPI_MOVE_* macros. (This is why a memcpy is not used)
 *
 ******************************************************************************/

void
AcpiRsMoveData (
    void                    *Destination,
    void                    *Source,
    UINT16                  ItemCount,
    UINT8                   MoveType)
{
    ACPI_NATIVE_UINT        i;


    /* One move per item */

    for (i = 0; i < ItemCount; i++)
    {
        switch (MoveType)
        {
        /*
         * For the 8-bit case, we can perform the move all at once
         * since there are no alignment or endian issues
         */
        case ACPI_RSC_MOVE8:
            ACPI_MEMCPY (Destination, Source, ItemCount);
            return;

        /*
         * 16-, 32-, and 64-bit cases must use the move macros that perform
         * endian conversion and/or accomodate hardware that cannot perform
         * misaligned memory transfers
         */
        case ACPI_RSC_MOVE16:
            ACPI_MOVE_16_TO_16 (&((UINT16 *) Destination)[i],
                                &((UINT16 *) Source)[i]);
            break;

        case ACPI_RSC_MOVE32:
            ACPI_MOVE_32_TO_32 (&((UINT32 *) Destination)[i],
                                &((UINT32 *) Source)[i]);
            break;

        case ACPI_RSC_MOVE64:
            ACPI_MOVE_64_TO_64 (&((UINT64 *) Destination)[i],
                                &((UINT64 *) Source)[i]);
            break;

        default:
            return;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetResourceInfo
 *
 * PARAMETERS:  ResourceType        - Byte 0 of a resource descriptor
 *
 * RETURN:      Pointer to the resource conversion handler
 *
 * DESCRIPTION: Extract the Resource Type/Name from the first byte of
 *              a resource descriptor.
 *
 ******************************************************************************/

ACPI_RESOURCE_INFO *
AcpiRsGetResourceInfo (
    UINT8                   ResourceType)
{
    ACPI_RESOURCE_INFO      *SizeInfo;


    ACPI_FUNCTION_ENTRY ();


    /* Determine if this is a small or large resource */

    if (ResourceType & ACPI_RESOURCE_NAME_LARGE)
    {
        /* Large Resource Type -- bits 6:0 contain the name */

        if (ResourceType > ACPI_RESOURCE_NAME_LARGE_MAX)
        {
            return (NULL);
        }

        SizeInfo = &AcpiGbl_LgResourceInfo [
                    (ResourceType & ACPI_RESOURCE_NAME_LARGE_MASK)];
    }
    else
    {
        /* Small Resource Type -- bits 6:3 contain the name */

        SizeInfo = &AcpiGbl_SmResourceInfo [
                    ((ResourceType & ACPI_RESOURCE_NAME_SMALL_MASK) >> 3)];
    }

    /* Zero entry indicates an invalid resource type */

    if (!SizeInfo->MinimumInternalStructLength)
    {
        return (NULL);
    }

    return (SizeInfo);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsSetResourceLength
 *
 * PARAMETERS:  TotalLength         - Length of the AML descriptor, including
 *                                    the header and length fields.
 *              Aml                 - Pointer to the raw AML descriptor
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set the ResourceLength field of an AML
 *              resource descriptor, both Large and Small descriptors are
 *              supported automatically. Note: Descriptor Type field must
 *              be valid.
 *
 ******************************************************************************/

void
AcpiRsSetResourceLength (
    ACPI_RSDESC_SIZE        TotalLength,
    AML_RESOURCE            *Aml)
{
    ACPI_RS_LENGTH          ResourceLength;


    ACPI_FUNCTION_ENTRY ();


    /* Determine if this is a small or large resource */

    if (Aml->SmallHeader.DescriptorType & ACPI_RESOURCE_NAME_LARGE)
    {
        /* Large Resource type -- bytes 1-2 contain the 16-bit length */

        ResourceLength = (ACPI_RS_LENGTH)
            (TotalLength - sizeof (AML_RESOURCE_LARGE_HEADER));

        /* Insert length into the Large descriptor length field */

        ACPI_MOVE_16_TO_16 (&Aml->LargeHeader.ResourceLength, &ResourceLength);
    }
    else
    {
        /* Small Resource type -- bits 2:0 of byte 0 contain the length */

        ResourceLength = (ACPI_RS_LENGTH)
            (TotalLength - sizeof (AML_RESOURCE_SMALL_HEADER));

        /* Insert length into the descriptor type byte */

        Aml->SmallHeader.DescriptorType = (UINT8)

            /* Clear any existing length, preserving descriptor type bits */

            ((Aml->SmallHeader.DescriptorType & ~ACPI_RESOURCE_NAME_SMALL_LENGTH_MASK)

            | ResourceLength);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsSetResourceHeader
 *
 * PARAMETERS:  DescriptorType      - Byte to be inserted as the type
 *              TotalLength         - Length of the AML descriptor, including
 *                                    the header and length fields.
 *              Aml                 - Pointer to the raw AML descriptor
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set the DescriptorType and ResourceLength fields of an AML
 *              resource descriptor, both Large and Small descriptors are
 *              supported automatically
 *
 ******************************************************************************/

void
AcpiRsSetResourceHeader (
    UINT8                   DescriptorType,
    ACPI_RSDESC_SIZE        TotalLength,
    AML_RESOURCE            *Aml)
{
    ACPI_FUNCTION_ENTRY ();


    /* Set the Descriptor Type */

    Aml->SmallHeader.DescriptorType = DescriptorType;

    /* Set the Resource Length */

    AcpiRsSetResourceLength (TotalLength, Aml);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsStrcpy
 *
 * PARAMETERS:  Destination         - Pointer to the destination string
 *              Source              - Pointer to the source string
 *
 * RETURN:      String length, including NULL terminator
 *
 * DESCRIPTION: Local string copy that returns the string length, saving a
 *              strcpy followed by a strlen.
 *
 ******************************************************************************/

static UINT16
AcpiRsStrcpy (
    char                    *Destination,
    char                    *Source)
{
    UINT16                  i;


    ACPI_FUNCTION_ENTRY ();


    for (i = 0; Source[i]; i++)
    {
        Destination[i] = Source[i];
    }

    Destination[i] = 0;

    /* Return string length including the NULL terminator */

    return ((UINT16) (i + 1));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetResourceSource
 *
 * PARAMETERS:  ResourceLength      - Length field of the descriptor
 *              MinimumLength       - Minimum length of the descriptor (minus
 *                                    any optional fields)
 *              ResourceSource      - Where the ResourceSource is returned
 *              Aml                 - Pointer to the raw AML descriptor
 *              StringPtr           - (optional) where to store the actual
 *                                    ResourceSource string
 *
 * RETURN:      Length of the string plus NULL terminator, rounded up to 32 bit
 *
 * DESCRIPTION: Copy the optional ResourceSource data from a raw AML descriptor
 *              to an internal resource descriptor
 *
 ******************************************************************************/

ACPI_RS_LENGTH
AcpiRsGetResourceSource (
    ACPI_RS_LENGTH          ResourceLength,
    ACPI_RS_LENGTH          MinimumLength,
    ACPI_RESOURCE_SOURCE    *ResourceSource,
    AML_RESOURCE            *Aml,
    char                    *StringPtr)
{
    ACPI_RSDESC_SIZE        TotalLength;
    UINT8                   *AmlResourceSource;


    ACPI_FUNCTION_ENTRY ();


    TotalLength = ResourceLength + sizeof (AML_RESOURCE_LARGE_HEADER);
    AmlResourceSource = ((UINT8 *) Aml) + MinimumLength;

    /*
     * ResourceSource is present if the length of the descriptor is longer than
     * the minimum length.
     *
     * Note: Some resource descriptors will have an additional null, so
     * we add 1 to the minimum length.
     */
    if (TotalLength > (ACPI_RSDESC_SIZE )(MinimumLength + 1))
    {
        /* Get the ResourceSourceIndex */

        ResourceSource->Index = AmlResourceSource[0];

        ResourceSource->StringPtr = StringPtr;
        if (!StringPtr)
        {
            /*
             * String destination pointer is not specified; Set the String
             * pointer to the end of the current ResourceSource structure.
             */
            ResourceSource->StringPtr = (char *)
                ((UINT8 *) ResourceSource) + sizeof (ACPI_RESOURCE_SOURCE);
        }

        /*
         * In order for the StructSize to fall on a 32-bit boundary, calculate
         * the length of the string (+1 for the NULL terminator) and expand the
         * StructSize to the next 32-bit boundary.
         *
         * Zero the entire area of the buffer.
         */
        TotalLength = ACPI_ROUND_UP_TO_32BITS (
                        ACPI_STRLEN ((char *) &AmlResourceSource[1]) + 1);
        ACPI_MEMSET (ResourceSource->StringPtr, 0, TotalLength);

        /* Copy the ResourceSource string to the destination */

        ResourceSource->StringLength = AcpiRsStrcpy (ResourceSource->StringPtr,
                                        (char *) &AmlResourceSource[1]);

        return ((ACPI_RS_LENGTH) TotalLength);
    }
    else
    {
        /* ResourceSource is not present */

        ResourceSource->Index = 0;
        ResourceSource->StringLength = 0;
        ResourceSource->StringPtr = NULL;
        return (0);
    }
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiRsSetResourceSource
 *
 * PARAMETERS:  Aml                 - Pointer to the raw AML descriptor
 *              MinimumLength       - Minimum length of the descriptor (minus
 *                                    any optional fields)
 *              ResourceSource      - Internal ResourceSource

 *
 * RETURN:      Total length of the AML descriptor
 *
 * DESCRIPTION: Convert an optional ResourceSource from internal format to a
 *              raw AML resource descriptor
 *
 ******************************************************************************/

ACPI_RSDESC_SIZE
AcpiRsSetResourceSource (
    AML_RESOURCE            *Aml,
    ACPI_RS_LENGTH          MinimumLength,
    ACPI_RESOURCE_SOURCE    *ResourceSource)
{
    UINT8                   *AmlResourceSource;
    ACPI_RSDESC_SIZE        DescriptorLength;


    ACPI_FUNCTION_ENTRY ();


    DescriptorLength = MinimumLength;

    /* Non-zero string length indicates presence of a ResourceSource */

    if (ResourceSource->StringLength)
    {
        /* Point to the end of the AML descriptor */

        AmlResourceSource = ((UINT8 *) Aml) + MinimumLength;

        /* Copy the ResourceSourceIndex */

        AmlResourceSource[0] = (UINT8) ResourceSource->Index;

        /* Copy the ResourceSource string */

        ACPI_STRCPY ((char *) &AmlResourceSource[1],
                ResourceSource->StringPtr);

        /*
         * Add the length of the string (+ 1 for null terminator) to the
         * final descriptor length
         */
        DescriptorLength += ((ACPI_RSDESC_SIZE) ResourceSource->StringLength + 1);
    }

    /* Return the new total length of the AML descriptor */

    return (DescriptorLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetPrtMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _PRT value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetPrtMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetPrtMethodData");


    /* Parameters guaranteed valid by caller */

    /* Execute the method, no parameters */

    Status = AcpiUtEvaluateObject (Handle, METHOD_NAME__PRT,
                ACPI_BTYPE_PACKAGE, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Create a resource linked list from the byte stream buffer that comes
     * back from the _CRS method execution.
     */
    Status = AcpiRsCreatePciRoutingTable (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by EvaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetCrsMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _CRS value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetCrsMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetCrsMethodData");


    /* Parameters guaranteed valid by caller */

    /* Execute the method, no parameters */

    Status = AcpiUtEvaluateObject (Handle, METHOD_NAME__CRS,
                ACPI_BTYPE_BUFFER, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Make the call to create a resource linked list from the
     * byte stream buffer that comes back from the _CRS method
     * execution.
     */
    Status = AcpiRsCreateResourceList (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by evaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetPrsMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _PRS value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetPrsMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetPrsMethodData");


    /* Parameters guaranteed valid by caller */

    /* Execute the method, no parameters */

    Status = AcpiUtEvaluateObject (Handle, METHOD_NAME__PRS,
                ACPI_BTYPE_BUFFER, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Make the call to create a resource linked list from the
     * byte stream buffer that comes back from the _CRS method
     * execution.
     */
    Status = AcpiRsCreateResourceList (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by evaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              Path            - Path to method, relative to Handle
 *              RetBuffer       - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _CRS or _PRS value of an
 *              object contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsGetMethodData (
    ACPI_HANDLE             Handle,
    char                    *Path,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsGetMethodData");


    /* Parameters guaranteed valid by caller */

    /* Execute the method, no parameters */

    Status = AcpiUtEvaluateObject (Handle, Path, ACPI_BTYPE_BUFFER, &ObjDesc);
    if (ACPI_FAILURE (Status)) {
        return_ACPI_STATUS (Status);
    }

    /*
     * Make the call to create a resource linked list from the
     * byte stream buffer that comes back from the method
     * execution.
     */
    Status = AcpiRsCreateResourceList (ObjDesc, RetBuffer);

    /* On exit, we must delete the object returned by EvaluateObject */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiRsSetSrsMethodData
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              InBuffer        - a pointer to a buffer structure of the
 *                                  parameter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to set the _SRS of an object contained
 *              in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsSetSrsMethodData (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *InBuffer)
{
    ACPI_PARAMETER_INFO     Info;
    ACPI_OPERAND_OBJECT     *Params[2];
    ACPI_STATUS             Status;
    ACPI_BUFFER             Buffer;


    ACPI_FUNCTION_TRACE ("RsSetSrsMethodData");


    /* Parameters guaranteed valid by caller */

    /*
     * The InBuffer parameter will point to a linked list of
     * resource parameters.  It needs to be formatted into a
     * byte stream to be sent in as an input parameter to _SRS
     *
     * Convert the linked list into a byte stream
     */
    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiRsCreateAmlResources (InBuffer->Pointer, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Init the param object */

    Params[0] = AcpiUtCreateInternalObject (ACPI_TYPE_BUFFER);
    if (!Params[0])
    {
        AcpiOsFree (Buffer.Pointer);
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Set up the parameter object */

    Params[0]->Buffer.Length  = (UINT32) Buffer.Length;
    Params[0]->Buffer.Pointer = Buffer.Pointer;
    Params[0]->Common.Flags   = AOPOBJ_DATA_VALID;
    Params[1] = NULL;

    Info.Node = Handle;
    Info.Parameters = Params;
    Info.ParameterType = ACPI_PARAM_ARGS;

    /* Execute the method, no return value */

    Status = AcpiNsEvaluateRelative (METHOD_NAME__SRS, &Info);
    if (ACPI_SUCCESS (Status))
    {
        /* Delete any return object (especially if ImplicitReturn is enabled) */

        if (Info.ReturnObject)
        {
            AcpiUtRemoveReference (Info.ReturnObject);
        }
    }

    /* Clean up and return the status from AcpiNsEvaluateRelative */

    AcpiUtRemoveReference (Params[0]);
    return_ACPI_STATUS (Status);
}

